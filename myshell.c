#include <stdio.h>    // Standart giriş/çıkış (printf, getline)
#include <stdlib.h>   // Genel amaçlı fonksiyonlar (malloc, exit)
#include <unistd.h>   // POSIX sistem çağrıları (fork, execvp, chdir, pipe, dup2)
#include <sys/wait.h> // waitpid
#include <string.h>   // Dize işleme (strtok, strcmp)
#include <fcntl.h>    // Dosya açma (open)
#include <sys/stat.h> // Dosya izinleri (open mode)

// Sabitler
#define TOKEN_DELIM " \t\r\n\a" // Ayrıcı karakterler (boşluk, tab, yeni satır)
#define TOKEN_BUFSIZE 64        // Argüman dizisi başlangıç boyutu

// Fonksiyon prototipleri
void shell_loop(void);
char *shell_read_line(void);
char **shell_split_line(char *line);
int shell_execute(char **args);
int shell_execute_pipe(char **args_left, char **args_right);

// Dahili Komutlar (Built-in)
int shell_cd(char **args);
int shell_exit(char **args);

char *builtin_str[] = {
    "cd",
    "exit"
};

int (*builtin_func[]) (char **) = {
    &shell_cd,
    &shell_exit
};

int shell_num_builtins() {
    return sizeof(builtin_str) / sizeof(char *);
}

// ----------------------------------------------------
// 1. TEMEL İŞLEVLER
// ----------------------------------------------------

/**
 * Kullanıcıdan bir satır okur (getline kullanarak)
 */
char *shell_read_line(void) {
    char *line = NULL;
    size_t bufsize = 0; 
    
    if (getline(&line, &bufsize, stdin) == -1) {
        if (feof(stdin)) {
            exit(EXIT_SUCCESS); // EOF (Ctrl+D) ile çıkış
        } else  {
            perror("myshell: getline hatası");
            exit(EXIT_FAILURE);
        }
    }
    return line;
}

/**
 * Gelen komut satırını ayırıcı karakterlere göre böler ve argüman dizisi döndürür.
 */
char **shell_split_line(char *line) {
    int bufsize = TOKEN_BUFSIZE, position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    if (!tokens) {
        fprintf(stderr, "myshell: bellek tahsis hatası\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, TOKEN_DELIM);

    while (token != NULL) {
        tokens[position] = token;
        position++;

        if (position >= bufsize) {
            bufsize += TOKEN_BUFSIZE;
            tokens = realloc(tokens, bufsize * sizeof(char*));
            if (!tokens) {
                fprintf(stderr, "myshell: bellek tahsis hatası\n");
                exit(EXIT_FAILURE);
            }
        }
        
        token = strtok(NULL, TOKEN_DELIM);
    }
    
    tokens[position] = NULL;
    return tokens;
}

// ----------------------------------------------------
// 2. DAHİLİ KOMUTLAR
// ----------------------------------------------------

int shell_cd(char **args) {
    if (args[1] == NULL) {
        // chdir(getenv("HOME")); // Eğer Home dizinine gitmek istenirse
        fprintf(stderr, "myshell: \"cd\" komutu için argüman eksik.\n");
    } else {
        if (chdir(args[1]) != 0) {
            perror("myshell");
        }
    }
    return 1;
}

int shell_exit(char **args) {
    return 0; // 0 döndürmek, ana döngüyü sonlandırır
}


// ----------------------------------------------------
// 3. PIPE İŞLEVİ
// ----------------------------------------------------

/**
 * İki komut arasında pipe (|) kullanarak veri akışı sağlar.
 */
int shell_execute_pipe(char **args_left, char **args_right) {
    int pipefd[2]; 
    pid_t p1, p2;

    if (pipe(pipefd) == -1) {
        perror("myshell: pipe hatası");
        return 1;
    }

    p1 = fork();
    if (p1 == 0) {
        // Çocuk 1 (Yazar): STDOUT'u pipe'ın yazma ucuna yönlendirir
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]); 

        if (execvp(args_left[0], args_left) == -1) {
            perror("myshell: execvp (pipe sol)");
            exit(EXIT_FAILURE);
        }
    } else if (p1 < 0) {
        perror("myshell: fork hatası");
        return 1;
    }

    p2 = fork();
    if (p2 == 0) {
        // Çocuk 2 (Okuyucu): STDIN'i pipe'ın okuma ucuna yönlendirir
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);

        if (execvp(args_right[0], args_right) == -1) {
            perror("myshell: execvp (pipe sağ)");
            exit(EXIT_FAILURE);
        }
    } else if (p2 < 0) {
        perror("myshell: fork hatası");
        return 1;
    }
    
    // Ana Proses: Pipe uçlarını kapatır
    close(pipefd[0]);
    close(pipefd[1]);

    // Her iki çocuğun da bitmesini bekle
    waitpid(p1, NULL, 0);
    waitpid(p2, NULL, 0);

    return 1;
}


// ----------------------------------------------------
// 4. ANA ÇALIŞTIRMA VE REDIRECTION İŞLEVİ
// ----------------------------------------------------

/**
 * Komutları çalıştıran ana fonksiyondur (Pipe, Redirection, Dahili ve Harici).
 */
int shell_execute(char **args) {
    // Girdi boşsa devam et
    if (args[0] == NULL) {
        return 1;
    }

    // ********** 1. PIPE KONTROLÜ **********
    int i = 0;
    while (args[i] != NULL) {
        if (strcmp(args[i], "|") == 0) {
            char **args_left = args;
            char **args_right = &args[i + 1]; 
            args[i] = NULL; 
            return shell_execute_pipe(args_left, args_right);
        }
        i++;
    }

    // ********** 2. REDIRECTION (YÖNLENDİRME) KONTROLÜ **********
    int fd;
    i = 0;
    int stdin_yedek = dup(STDIN_FILENO); // STDIN'i yedekle
    int stdout_yedek = dup(STDOUT_FILENO); // STDOUT'u yedekle

    while (args[i] != NULL) {
        if (strcmp(args[i], ">") == 0) {
            if (args[i + 1] == NULL) { fprintf(stderr, "myshell: Dosya adı eksik.\n"); return 1; }
            
            // Dosyayı aç ve STDOUT'a yönlendir
            fd = open(args[i + 1], O_WRONLY | O_TRUNC | O_CREAT, 0644);
            dup2(fd, STDOUT_FILENO);
            close(fd);

            args[i] = NULL; // Operatörü ve dosya adını sil
            break; 
        } 
        else if (strcmp(args[i], "<") == 0) {
            if (args[i + 1] == NULL) { fprintf(stderr, "myshell: Dosya adı eksik.\n"); return 1; }

            // Dosyayı aç ve STDIN'e yönlendir
            fd = open(args[i + 1], O_RDONLY);
            dup2(fd, STDIN_FILENO);
            close(fd);

            args[i] = NULL; // Operatörü ve dosya adını sil
            break;
        }
        i++;
    }


    // ********** 3. DAHİLİ KOMUT KONTROLÜ **********
    for (i = 0; i < shell_num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            // Yönlendirme yapıldıysa bile, dahili komutlar yönlendirmeyi kullanmaz
            int result = (*builtin_func[i])(args);
            
            // Yönlendirmeyi geri al (önemli!)
            dup2(stdout_yedek, STDOUT_FILENO); 
            dup2(stdin_yedek, STDIN_FILENO);
            close(stdout_yedek);
            close(stdin_yedek);
            return result;
        }
    }


    // ********** 4. HARİCİ KOMUT ÇALIŞTIRMA **********
    pid_t pid = fork();

    if (pid == 0) {
        // Çocuk Prosesi: Komutu çalıştırır
        if (execvp(args[0], args) == -1) {
            perror("myshell"); 
        }
        exit(EXIT_FAILURE); 
    } else if (pid < 0) {
        perror("myshell");
    } else {
        // Ana Proses: Çocuk prosesin bitmesini bekler
        int status;
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    
    // Yönlendirmeyi geri al (Harici komut bittiğinde de geri al)
    dup2(stdout_yedek, STDOUT_FILENO);
    dup2(stdin_yedek, STDIN_FILENO);
    close(stdout_yedek);
    close(stdin_yedek);


    return 1;
}


// ----------------------------------------------------
// ANA DÖNGÜ VE MAIN FONKSİYONU
// ----------------------------------------------------

/**
 * Ana Shell Döngüsü (Read-Eval-Print Loop - REPL)
 */
void shell_loop(void) {
    char *line;
    char **args;
    int status;

    do {
        printf("myshell> ");
        line = shell_read_line();
        args = shell_split_line(line);
        status = shell_execute(args);

        free(line);
        free(args);

    } while (status); 
}

/**
 * Programın başlangıç noktası
 */
int main(int argc, char **argv) {
    shell_loop();
    return EXIT_SUCCESS;
}
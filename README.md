Simple Shell Implementation (C Language)
Bu proje, İşletim Sistemleri dersi kapsamında geliştirilmiş, C dili tabanlı temel bir kabuk (shell) yazılımıdır. Projenin odak noktası, Unix benzeri işletim sistemlerinde süreç yönetimi, sistem çağrıları ve komut yürütme süreçlerinin mantığını kavramaktır.

🚀 Öne Çıkan Özellikler
Süreç Yönetimi: fork() ve execvp() sistem çağrılarını kullanarak komutların yeni süreçlerde çalıştırılması.

Giriş/Çıkış Yönlendirme: Komut çıktılarını dosyalara yönlendirme (I/O Redirection) desteği.

Komut Desteği: Standart Linux terminal komutlarının (ls, clear, echo vb.) yürütülmesi.

Hata Yönetimi: Geçersiz komutlar veya sistem çağrısı hataları için temel hata yakalama mekanizması.

📂 Dosya Yapısı
myshell.c: Ana kaynak kodu ve sistem mantığı.

log.txt & sonuc.txt: Komut çıktılarını ve yönlendirme işlemlerini test etmek amacıyla kullanılan çalışma dosyaları.

💻 Çalıştırma Talimatları
Kaynak kodunu derleyin:

Bash
gcc myshell.c -o myshell
Kabuğu çalıştırın:

Bash
./myshell

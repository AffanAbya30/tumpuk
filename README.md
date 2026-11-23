# Tumpuk

Tumpuk adalah gim arkade 3D untuk PC yang dikembangkan sebagai proyek akhir mata kuliah Komputer Grafis. Kami memilih mekanik menyusun balok karena aturannya mudah dipahami dan memberi ruang untuk menerapkan materi yang telah dipelajari, seperti pemodelan 3D, transformasi, proyeksi ortografik, animasi, dan interaksi pengguna.

Jatuhkan balok saat sejajar dengan balok di bawahnya. Jika penempatannya sedikit meleset, bagian yang tidak bertumpuk akan terpotong dan jatuh. Bagian yang tersisa menjadi pijakan untuk balok berikutnya. Permainan berakhir saat kedua balok sama sekali tidak bertumpuk.

Pratinjau menu Tumpuk dari versi awal

## Cara Main

| Kontrol | Aksi |
|---|---|
| Enter atau klik Mulai | Mulai permainan |
| Space atau klik arena | Jatuhkan balok |
| P, Esc, atau klik Jeda | Jeda permainan |
| Enter atau klik Lanjutkan | Lanjutkan permainan |
| R atau klik Coba lagi | Mulai ulang setelah permainan selesai |
| C atau klik Kredit dari menu utama | Lihat anggota kelompok |

Balok bergerak bergantian pada sumbu X dan Z. Penempatan yang cukup dekat dengan pusat akan terkunci pas. Beberapa penempatan pas berturut-turut menghasilkan combo, dan balok bergerak semakin cepat seiring kenaikan skor.

## Menjalankan Gim

### Windows

Untuk melakukan build dari kode sumber, siapkan Visual Studio 2022 dengan workload Desktop development with C++, termasuk MSVC x64 dan Windows SDK.

1. Clone repository atau ekstrak ZIP ke satu folder.
2. Jalankan Build-Windows.bat.
3. Buka dist\Tumpuk\Tumpuk.exe setelah build berhasil.

Alternatifnya, buka Tumpuk.sln di Visual Studio, pilih konfigurasi Release dan x64, lalu pilih Build > Build Solution. Executable akan tersedia di dist\Release\Tumpuk.exe.

Untuk membagikan build Windows, sertakan Tumpuk.exe, credits.txt, dan LICENSE-fonts.txt dalam satu folder. Pemain dapat menjalankan build tersebut secara offline tanpa memasang Visual Studio, GLEW, atau FreeGLUT.

### Linux

Siapkan compiler yang mendukung C++17, CMake, alat build seperti Make atau Ninja, serta paket pengembangan OpenGL dan FreeGLUT. Jalankan dari folder proyek:

cmake -S . -B build-glut -DTUMPUK_FREEGLUT=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-glut
./build-glut/Tumpuk

## Penerapan Komputer Grafis

| Materi | Penerapan di Tumpuk |
|---|---|
| Geometri 3D | Balok dibentuk dari enam bidang. Perbedaan warna dan gradasi antarsisi membantu menunjukkan kedalaman. |
| Proyeksi dan kamera | Proyeksi ortografik dengan sudut isometrik menjaga skala balok. Kamera mengikuti tinggi menara, sementara antarmuka 2D tetap di koordinat layar. |
| Perhitungan geometri | Saat balok dijatuhkan, gim menghitung irisan antara balok aktif dan penyangganya pada sumbu gerak. Irisan itu menentukan ukuran serta posisi balok berikutnya. |
| Transformasi dan animasi | Potongan bergerak jatuh sambil berputar. Permukaan lantai menjadi batas visual, sehingga bagian potongan yang sudah melewatinya tidak lagi terlihat. |
| Interaksi | Keyboard dan mouse mengontrol permainan. Tampilan menjaga rasio 16:9 saat ukuran jendela berubah dan menyesuaikan koordinat klik dengan area gambar. |

Tumpuk ditulis dalam C++17 dan OpenGL, dengan backend Win32 untuk Windows dan FreeGLUT untuk Linux. Aturan permainan ada di src/game.hpp, sedangkan render dan antarmuka ada di src/app.cpp.

## Desain Antarmuka

Menu utama menempatkan judul dan tombol di kiri, serta menara isometrik di kanan. Menara itu berangkat dari bentuk logo tiga lapis, lalu mendemonstrasikan mekanik pemotongan balok yang digunakan di arena. Font Outfit, palet mint dan lavender, serta tombol bersudut 90° menyatukan tampilan menu, Kredit, arena, jeda, dan hasil.

## Kredit

Nama tim: Ketuanya Ijal ajah

- Faizal Reza Abdillah — ketua kelompok
- Ozora Nathalnael Purba
- Affan Abyarahman
- Neval Pradipta Harimurti

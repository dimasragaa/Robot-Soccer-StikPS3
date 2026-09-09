# Robot Kontrol PS3 — Struktur File

Program yang tadinya satu file panjang, sekarang dipecah per fungsi.
**Aturan sederhana: mau ubah sesuatu → buka file yang namanya sesuai.**

## Struktur file

| File | Isi | Buka kalau mau ubah... |
|------|-----|------------------------|
| **Config.h** | Semua angka & pin | pin motor, frekuensi PWM, kecepatan, durasi sequence, MAC PS3, timing — **hampir semua kalibrasi ada di sini** |
| **Controls.cpp** | Logika tombol & stik | tombol apa melakukan apa, mapping stik, pemicu sequence |
| **Menu.cpp** | Isi menu & preset mode | teks menu, nilai tiap mode (Normal/Dribble/Attack/Defense), batas pengaturan |
| **Sequence.cpp** | Gerakan otomatis | cara kerja mundur→maju / putar kanan / putar kiri |
| **Motor.cpp** | Kendali driver BTN | cara pin motor digerakkan (jarang diubah) |
| **Kicker.cpp** | Mekanisme tendang | logika solenoid tendang |
| **Display.cpp** | Tampilan OLED | teks & tata letak layar OLED |
| **Debug.cpp** | Cetak Serial Monitor | format info yang tampil di serial |
| **State.h / State.cpp** | Variabel status bersama | (jarang disentuh) daftar variabel runtime |
| **main.cpp** | setup() & loop() | urutan startup — merangkai semua modul |

## Contoh cepat

- **Mau ganti pin motor?** → `Config.h`, bagian "PIN MOTOR".
- **Mau L2 jadi 60%?** → `Config.h`, ubah `SPD_L2`.
- **Mau tombol X jadi gerakan lain?** → `Controls.cpp`, fungsi `handleRun()`.
- **Mau mundurnya lebih lama?** → `Config.h`, ubah `SEQX_MUNDUR_MS`.
- **Mau default konek jadi mode Dribble?** → `Config.h`, ubah `DEFAULT_ITEM` jadi `1`.
- **OLED belum dipasang?** → `Config.h`, set `USE_OLED 0`.

## Alur singkat

`main.cpp loop()` memanggil tiap tick:
`controlsUpdate()` (Controls) → menangani tombol, memanggil `runSequence()` (Sequence)
atau kontrol manual, yang menggerakkan `setMotorL/R()` (Motor).
Lalu `displayTick()` (Display) dan `debugTick()` (Debug) memperbarui layar & serial.
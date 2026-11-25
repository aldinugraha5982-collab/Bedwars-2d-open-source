# Bedwars PC (C++ / SDL2)

Versi desktop single-player yang menulis ulang inti game HTML: empat tim dengan bot, generator resource, bed yang bisa dihancurkan, bridging, pertarungan, toko, TNT, fireball, dan upgrade tim. Grafis dan UI dibuat ulang sederhana. Fitur lobby serta multiplayer WebRTC dari versi HTML tidak disertakan.

## Build

Perlu CMake 3.16+, compiler C++17, dan SDL2 development package.

```powershell
cmake -S . -B build -DSDL2_DIR="C:/path/to/SDL2/lib/cmake/SDL2"
cmake --build build --config Release
```

Jalankan `build/bedwars.exe` di Windows. Jika memakai generator MinGW, tambahkan `-G "MinGW Makefiles" -DCMAKE_CXX_COMPILER=g++` saat konfigurasi.

## Kontrol

- `WASD` atau tombol panah: bergerak
- `Space` / `J` / klik kiri: menyerang
- `1`-`6`: pilih item hotbar
- `E` / klik kanan: gunakan item atau pasang blok
- `B` / `Esc`: buka/tutup toko
- `T`: hidup/matikan auto-bridge
- Di toko: `Q`-`O` membeli item; `1`-`3` membeli upgrade tim

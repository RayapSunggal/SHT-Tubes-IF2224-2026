# Lexical Analyzer — Tugas Besar IF2224

Implementasi *Lexical Analyzer* (Lexer/Tokenizer) untuk bahasa pemrograman **Arion** menggunakan pendekatan **Deterministic Finite Automaton (DFA)** berbasis *Mesin Karakter*.

## Identitas Kelompok

| NIM | Nama Lengkap | 
|-----|--------------|
| 13524010 | Audric Yusuf Maynard Simatupang |
| 13524052 | Raynard Fausta |
| 13524062 | Nathan Edward C Marpaung |
| 13524112 | Richard Samuel Simanullang |

> **Kelas**: K2  
> **Kode Kelompok**: SHT

---

## Deskripsi Program

Program ini merupakan *Lexical Analyzer* yang memproses source code bahasa pemrograman **Arion** dan memecahnya menjadi deretan **token**. Lexer membaca file input karakter per karakter menggunakan abstraksi *Mesin Karakter* (`mesinkarakter`) dan mengenali token berdasarkan aturan DFA.

### Daftar Token yang Dikenali

| No | Token | Definisi | Contoh |
|----|-------|----------|--------|
| 1 | `intcon` | Konstanta integer | `1`, `3`, `48` |
| 2 | `realcon` | Konstanta bilangan riil | `3.14`, `26.7` |
| 3 | `charcon` | Konstanta karakter singular, diapit petik tunggal (`'`) | `'j'`, `'k'` |
| 4 | `string` | Sekuens karakter, diapit petik tunggal (`'`) | `'IRK'`, `'TBFO'` |
| 5 | `notsy` | Operator logika NOT (negasi) | `NOT` |
| 6 | `plus` | Operator aritmatika pertambahan | `+` |
| 7 | `minus` | Operator aritmatika pengurangan | `-` |
| 8 | `times` | Operator aritmatika perkalian | `*` |
| 9 | `idiv` | Operator pembagian integer | `div` |
| 10 | `rdiv` | Operator pembagian riil | `/` |
| 11 | `imod` | Operator aritmatika modulo | `MOD` |
| 12 | `andsy` | Operator logika AND | `AND` |
| 13 | `orsy` | Operator logika OR | `OR` |
| 14 | `eql` | Equal | `==` |
| 15 | `neq` | Not equal | `<>` |
| 16 | `gtr` | Greater than | `>` |
| 17 | `geq` | Greater than or equal | `>=` |
| 18 | `lss` | Less than | `<` |
| 19 | `leq` | Less than or equal | `<=` |
| 20 | `lparent` | Left parentheses | `(` |
| 21 | `rparent` | Right parentheses | `)` |
| 22 | `lbrack` | Kurung siku kiri | `[` |
| 23 | `rbrack` | Kurung siku kanan | `]` |
| 24 | `comma` | Koma | `,` |
| 25 | `semicolon` | Titik koma | `;` |
| 26 | `period` | Titik | `.` |
| 27 | `colon` | Titik dua | `:` |
| 28 | `becomes` | Assignment operator | `:=` |
| 29 | `constsy` | Deklarasi konstanta | `const` |
| 30 | `typesy` | Deklarasi tipe data | `type` |
| 31 | `varsy` | Deklarasi variabel | `var` |
| 32 | `functionsy` | Deklarasi fungsi | `function` |
| 33 | `proceduresy` | Deklarasi prosedur | `procedure` |
| 34 | `arraysy` | Deklarasi array | `array` |
| 35 | `recordsy` | Deklarasi record | `record` |
| 36 | `programsy` | Deklarasi program | `program` |
| 37 | `ident` | Identifier (case-insensitive, diawali huruf, diikuti huruf/angka) | `x`, `PI`, `MyInt` |
| 38 | `beginsy` | Awal blok | `begin` |
| 39 | `ifsy` | Percabangan if | `if` |
| 40 | `casesy` | Percabangan case | `case` |
| 41 | `repeatsy` | Perulangan repeat | `repeat` |
| 42 | `whilesy` | Perulangan while | `while` |
| 43 | `forsy` | Perulangan for | `for` |
| 44 | `endsy` | Akhir blok | `end` |
| 45 | `elsesy` | Percabangan else | `else` |
| 46 | `untilsy` | Akhir repeat | `until` |
| 47 | `ofsy` | Kata kunci of | `of` |
| 48 | `dosy` | Kata kunci do | `do` |
| 49 | `tosy` | Kata kunci to | `to` |
| 50 | `downtosy` | Kata kunci downto | `downto` |
| 51 | `thensy` | Kata kunci then | `then` |
| 52 | `comment` | Komentar, diawali `{` atau `(*`, diakhiri `}` atau `*)` | `{ komentar }`, `(* komentar *)` |

### Arsitektur

```
┌────────────┐     ┌────────────────┐     ┌───────┐     ┌───────┐
│  File .txt │────▶│ Mesin Karakter │────▶│ Lexer │────▶│ Token │
│  (input)   │     │  (CC, ADV)     │     │ (DFA) │     │ (out) │
└────────────┘     └────────────────┘     └───────┘     └───────┘
```

- **Mesin Karakter** (`mesinkarakter`): Abstraksi pembacaan karakter satu per satu dari file. Menyediakan variabel global `CC` (karakter saat ini) dan `EOP` (end-of-process), serta prosedur `ADV()` untuk maju ke karakter berikutnya.
- **Lexer**: Mesin DFA yang mengonsumsi karakter dari *Mesin Karakter* dan menghasilkan token berdasarkan transisi state.
- **Token**: Struktur data berisi `TokenType` dan `lexeme` (representasi teks dari token).
- **FileIO**: Utilitas untuk membangun path file input/output.

---

## Requirements

- **Compiler**: GCC dengan dukungan C11
  - Windows: [MinGW-w64](https://www.mingw-w64.org/) atau melalui [MSYS2](https://www.msys2.org/)
  - Linux/macOS: GCC sudah tersedia secara default
- **Build Tool**: [GNU Make](https://www.gnu.org/software/make/)
- **Sistem Operasi**: Windows, Linux, atau macOS

---

## Cara Instalasi dan Penggunaan Program

### 1. Clone Repository

```bash
git clone <url-repository>
cd SHT-Tubes-IF2224-2026
```

### 2. Build Program

```bash
make build
```

Perintah ini akan mengompilasi seluruh source code dan menghasilkan executable `app` (atau `app.exe` di Windows).

### 3. Jalankan Program

```bash
make run
```

Atau jalankan executable secara langsung:

```bash
./app
```

### 4. Penggunaan

Setelah program dijalankan:

1. Masukkan nama file input (file `.txt` di dalam folder `test/milestone-1/input/`).  
   Ekstensi `.txt` boleh ditulis maupun tidak.
   ```
   Masukkan nama file input (ketik 'exit' untuk berhenti): contoh
   ```
2. Masukkan nama file output (akan disimpan di `test/milestone-1/output/`).
   ```
   Masukkan nama file output: hasil
   ```
3. Program akan menampilkan hasil tokenisasi di terminal dan menyimpannya ke file output.
4. Ketik `exit` untuk menghentikan program.

### Contoh Output

```
Hasil tokenisasi:

programsy
ident (contoh)
semicolon
beginsy
endsy
period
```

### 5. Membersihkan Build

```bash
make clean
```

---

## Struktur Direktori

```
SHT-Tubes-IF2224-2026/
├── Makefile
├── README.md
├── app                          # Executable (hasil build)
├── src/
│   ├── main.c                   # Entry point program
│   ├── fileio/
│   │   ├── fileio.h             # Header utilitas file I/O
│   │   └── fileio.c             # Implementasi utilitas file I/O
│   ├── lexer/
│   │   ├── lexer.h              # Header lexer & definisi DFA states
│   │   └── lexer.c              # Implementasi DFA lexer
│   ├── mesinkarakter/
│   │   ├── mesinkarakter.h      # Header mesin karakter
│   │   └── mesinkarakter.c      # Implementasi mesin karakter
│   └── token/
│       ├── token.h              # Definisi TokenType & struct Token
│       └── token.c              # Konversi TokenType ke string
└── test/
    └── milestone-1/
        ├── input/               # File input (.txt)
        └── output/              # File output hasil tokenisasi
```

---

## Pembagian Tugas

| NIM | Tugas |
|---------|-------|
| 13524010 | Merancang DFA bagian keyword, Gambar DFA |
| 13524052 | Merancang DFA bagian karakter khusus, Implementasi program|
| 13524062 | Merancang DFA bagian karakter khusus, Gambar DFA |
| 13524112 | Merancang DFA bagian number dan kasus khusus, Laporan |

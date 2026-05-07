# Lexical and Syntax Analyzer - Tugas Besar IF2224

Implementasi *Lexical Analyzer* dan *Syntax Analyzer* untuk bahasa pemrograman **Arion**. Analisis leksikal menggunakan pendekatan **Deterministic Finite Automaton (DFA)** berbasis *Mesin Karakter*, sedangkan analisis sintaks menggunakan parser *recursive descent* dan validasi grammar untuk membentuk *parse tree*.

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

Program ini memproses source code bahasa pemrograman **Arion** melalui dua mode analisis utama, yaitu lexical analysis dan syntax analysis. Program menerima file teks berisi source code Arion, membaca isi file tersebut, lalu menghasilkan keluaran sesuai mode yang dipilih pengguna.

1. **Lexical Analysis**: membaca file input karakter per karakter menggunakan abstraksi *Mesin Karakter* (`mesinkarakter`), lalu mengelompokkan karakter menjadi deretan **token** berdasarkan aturan DFA. Token yang dihasilkan berisi jenis token, lexeme, dan informasi baris.
2. **Syntax Analysis**: menggunakan token hasil lexer sebagai masukan parser. Parser membaca token secara berurutan dengan metode *recursive descent*, mencocokkan susunan token terhadap grammar Arion, menangani error sintaks, lalu membangun **parse tree** jika program valid. Parse tree yang dihasilkan merepresentasikan struktur hierarkis program, mulai dari `<program>`, `<program-header>`, bagian deklarasi, compound statement, statement, hingga expression.

Pada mode syntax analysis, input tetap berupa source code Arion, bukan file token manual. Source code terlebih dahulu diproses oleh lexer, kemudian parser menggunakan token stream tersebut untuk membangun parse tree. Analisis sintaks memastikan struktur program mengikuti bentuk yang didukung, seperti header program, deklarasi konstanta, deklarasi tipe, deklarasi variabel, deklarasi procedure/function, compound statement, assignment, percabangan, perulangan, pemanggilan procedure/function, variable/component variable, dan ekspresi.

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

### Daftar Node dan Production yang Digunakan

Node berikut digunakan oleh parser pada mode syntax analysis untuk membangun parse tree. Setiap node merepresentasikan non-terminal atau bagian grammar bahasa Arion, sedangkan production menjelaskan susunan token dan node anak yang harus dipenuhi.

| No | Node | Production |
|----|------|------------|
| 1 | `<program>` | `<program-header> <declaration-part> <compound-statement> period` |
| 2 | `<program-header>` | `programsy ident semicolon` |
| 3 | `<declaration-part>` | `{<const-declaration>} {<type-declaration>} {<var-declaration>} {<subprogram-declaration>}` |
| 4 | `<const-declaration>` | `constsy {ident eql <constant> semicolon}+` |
| 5 | `<constant>` | `charcon | string | ident | intcon | realcon | (plus | minus) (ident | intcon | realcon)` |
| 6 | `<type-declaration>` | `typesy {ident eql <type> semicolon}+` |
| 7 | `<var-declaration>` | `varsy {<identifier-list> colon <type> semicolon}+` |
| 8 | `<identifier-list>` | `ident {comma ident}` |
| 9 | `<type>` | `ident | <array-type> | <range> | <enumerated> | <record-type>` |
| 10 | `<array-type>` | `arraysy lbrack (<range> | ident) rbrack ofsy <type>` |
| 11 | `<range>` | `<constant> period period <constant>` |
| 12 | `<enumerated>` | `lparent ident {comma ident} rparent` |
| 13 | `<record-type>` | `recordsy [<field-list>] endsy` |
| 14 | `<field-list>` | `<field-part> {semicolon <field-part>}` |
| 15 | `<field-part>` | `<identifier-list> colon <type>` |
| 16 | `<subprogram-declaration>` | `<procedure-declaration> | <function-declaration>` |
| 17 | `<procedure-declaration>` | `proceduresy ident [<formal-parameter-list>] semicolon block semicolon` |
| 18 | `<function-declaration>` | `functionsy ident [<formal-parameter-list>] colon ident semicolon block semicolon` |
| 19 | `block` | `<declaration-part> <compound-statement>` |
| 20 | `<formal-parameter-list>` | `lparent <parameter-group> {semicolon <parameter-group>} rparent` |
| 21 | `<parameter-group>` | `<identifier-list> colon (ident | <array-type>)` |
| 22 | `<compound-statement>` | `beginsy [<statement-list>] endsy` |
| 23 | `<statement-list>` | `<statement> {semicolon <statement>} [semicolon]` |
| 24 | `<statement>` | `<assignment-statement> | <compound-statement> | <if-statement> | <case-statement> | <while-statement> | <repeat-statement> | <for-statement> | <procedure/function-call>` |
| 25 | `<assignment-statement>` | `<variable> becomes <expression>` |
| 26 | `<if-statement>` | `ifsy <expression> thensy <statement> [elsesy <statement>]` |
| 27 | `<case-statement>` | `casesy <expression> ofsy <case-block> endsy` |
| 28 | `<case-block>` | `<constant> {comma <constant>} colon <statement> [semicolon <case-block>]` |
| 29 | `<while-statement>` | `whilesy <expression> dosy <statement>` |
| 30 | `<repeat-statement>` | `repeatsy <statement-list> untilsy <expression>` |
| 31 | `<for-statement>` | `forsy ident becomes <expression> (tosy | downtosy) <expression> dosy <statement>` |
| 32 | `<variable>` | `ident {<component-variable>}` |
| 33 | `<component-variable>` | `lbrack <index-list> rbrack | period ident` |
| 34 | `<index-list>` | `(intcon | charcon | ident) {comma <index-list>}` |
| 35 | `<procedure/function-call>` | `ident [lparent [<parameter-list>] rparent]` |
| 36 | `<parameter-list>` | `<expression> {comma <expression>}` |
| 37 | `<expression>` | `<simple-expression> [<relational-operator> <simple-expression>]` |
| 38 | `<simple-expression>` | `[plus | minus] <term> {<additive-operator> <term>}` |
| 39 | `<term>` | `<factor> {<multiplicative-operator> <factor>}` |
| 40 | `<factor>` | `intcon | realcon | charcon | string | <variable> | <procedure/function-call> | lparent <expression> rparent | notsy <factor>` |
| 41 | `<relational-operator>` | `eql | neq | gtr | geq | lss | leq` |
| 42 | `<additive-operator>` | `plus | minus | orsy` |
| 43 | `<multiplicative-operator>` | `times | rdiv | idiv | imod | andsy` |

### Arsitektur

```
+------------+     +----------------+     +-------+     +-------------+
| File .txt  | --> | Mesin Karakter | --> | Lexer | --> | Token Stream |
| (input)    |     | (CC, ADV, EOP) |     | (DFA) |     |             |
+------------+     +----------------+     +-------+     +-------------+
                                                                  |
                                                                  v
                                                        +------------------+
                                                        | Syntax Analyzer  |
                                                        | Recursive Descent|
                                                        +------------------+
                                                                  |
                                                                  v
                                                        +------------------+
                                                        | Grammar Validator|
                                                        +------------------+
                                                                  |
                                                                  v
                                                        +------------------+
                                                        | Parse Tree/Output|
                                                        +------------------+
```

- **Mesin Karakter** (`mesinkarakter`): Abstraksi pembacaan karakter satu per satu dari file. Menyediakan variabel global `CC` (karakter saat ini), `EOP` (end-of-process), dan prosedur `ADV()` untuk maju ke karakter berikutnya.
- **Lexer**: Mesin DFA yang mengonsumsi karakter dari *Mesin Karakter* dan menghasilkan token berdasarkan transisi state.
- **Token**: Struktur data berisi `TokenType`, `lexeme`, dan nomor baris.
- **Syntax Analyzer** (`parser`): Parser *recursive descent* yang membaca token dari lexer, menyimpan token dalam struktur internal `Parser`, lalu memanggil fungsi parsing sesuai non-terminal grammar. Contohnya, `parseProgram` menangani root program, `parseDeclarationPart` menangani deklarasi, `parseStatement` memilih jenis statement berdasarkan token awal, dan `parseExpression` menangani ekspresi dengan hierarki presedensi.
- **Error Handling Parser**: Bagian dari parser yang mencatat kesalahan sintaks dengan informasi baris dan token terdekat. Parser juga memiliki mekanisme pemulihan sederhana, seperti melewati token sampai batas statement, expression, atau type agar lebih dari satu error dapat dilaporkan.
- **Grammar Validator** (`grammar`): Pemeriksa struktur parse tree. Setiap node hasil parser dikonversi ke node grammar lalu divalidasi dengan fungsi `is_*_complete` agar susunannya sesuai production yang didukung.
- **Parse Tree** (`parse_tree`): Struktur pohon yang menyimpan label node dan child, lalu mencetak hasil analisis sintaks ke terminal dan file output.
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

Setelah program dijalankan, pilih mode analisis:

```text
Pilih mode analisis:
1. Lexical Analysis
2. Syntax Analysis
0. Exit
Masukkan pilihan:
```

Untuk **Lexical Analysis**:

1. Masukkan nama file input dari folder `test/milestone-1/input/`.
2. Masukkan nama file output yang akan disimpan ke folder `test/milestone-1/output/`.
3. Program menampilkan hasil tokenisasi di terminal dan menyimpannya ke file output.

Untuk **Syntax Analysis**:

1. Masukkan nama file input dari folder `test/milestone-2/input/`.
2. Program otomatis membuat file output dengan nama yang sama di folder `test/milestone-2/output/`.
3. Jika sintaks valid, program menampilkan pesan sukses dan parse tree, lalu menyimpan parse tree ke file output.
4. Jika sintaks tidak valid, program menampilkan pesan error beserta lokasi baris dan token terdekat.

Ketik `back` pada prompt nama file input untuk kembali ke pemilihan mode, atau pilih `0` untuk menghentikan program.

### Contoh Output Lexical Analysis

```
Hasil tokenisasi:

programsy
ident (contoh)
semicolon
beginsy
endsy
period
```

### Contoh Output Syntax Analysis

```text
Syntax analysis success.
Syntax analysis successful.

Parse tree:
<program>
|-- <program-header>
|   |-- programsy
|   |-- ident(contoh)
|   `-- semicolon
|-- <declaration-part>
|-- <compound-statement>
|   |-- beginsy
|   `-- endsy
`-- period
```

### 5. Membersihkan Build

```bash
make clean
```

---

## Struktur Direktori

```text
SHT-Tubes-IF2224-2026/
|-- .gitattributes
|-- Makefile
|-- README.md
|-- doc/
|   `-- Laporan-1-SHT.pdf
|-- src/
|   |-- main.c
|   |-- fileio/
|   |   |-- fileio.c
|   |   `-- fileio.h
|   |-- grammar/
|   |   |-- grammar.c
|   |   `-- grammar.h
|   |-- lexer/
|   |   |-- lexer.c
|   |   `-- lexer.h
|   |-- mesinkarakter/
|   |   |-- mesinkarakter.c
|   |   `-- mesinkarakter.h
|   |-- node/
|   |   `-- node.h
|   |-- parse_tree/
|   |   |-- parse_tree.c
|   |   `-- parse_tree.h
|   |-- parser/
|   |   |-- parser.c
|   |   `-- parser.h
|   `-- token/
|       |-- token.c
|       `-- token.h
`-- test/
    |-- milestone-1/
    |   |-- input/
    |   |   `-- .gitkeep
    |   `-- output/
    |       `-- .gitkeep
    `-- milestone-2/
        |-- input/
        |   |-- input1.txt
        |   |-- input2.txt
        |   |-- ...
        |   `-- input13.txt
        `-- output/
            |-- input1.txt
            |-- input2.txt
            |-- ...
            `-- input13.txt
```

---

## Pembagian Tugas

| Nama | NIM | Tugas |
|------|-----|-------|
| Audric Y. M.Simatupang | 13524010 | Testing source code, Pembuatan Laporan |
| Raynard Fausta | 13524052 | Pembuatan source code |
| Nathan E.C Marpaung | 13524062 | Testing source code, Pembuatan Laporan |
| Richard S Simanullang | 13524112 | Bugfixing dan revisi source code, testing source code, pembuatan hasil test case pada laporan |

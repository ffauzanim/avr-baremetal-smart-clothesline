# Bare-Metal AVR Smart Clothesline (Jemuran Otomatis 3 Sensor Berbasis Register ATmega328P)

![Microcontroller](https://img.shields.io/badge/MCU-ATmega328P%20(Arduino%20Uno)-00979D?style=for-the-badge&logo=arduino)
![Language](https://img.shields.io/badge/Language-C%2FC%2B%2B%20(Bare--Metal%20AVR)-00599C?style=for-the-badge&logo=cplusplus)
![Peripherals](https://img.shields.io/badge/Registers-ADC%20%7C%20Timer2%20ISR%20%7C%20UART-orange?style=for-the-badge)

Sistem purwarupa **Jemuran Pakaian Otomatis** berbasis mikrokontroler **ATmega328P (Arduino Uno R3)** yang memantau kondisi cuaca dan lingkungan secara *real-time* menggunakan **3 sensor** (Sensor Cahaya **LDR**, Sensor Gas/Asap **MQ-2**, dan Sensor Hujan **Raindrop**) serta penggerak aktuator **Motor Servo SG90**.

Proyek ini dirancang menggunakan pendekatan **Pemrograman Tingkat Register (Bare-Metal AVR)** tanpa bergantung pada fungsi abstraksi tinggi seperti `analogRead()`, `Serial.begin()`, `Serial.print()`, ataupun `delay()` yang bersifat *blocking*. Akuisisi analog dikendalikan langsung melalui register **ADC (`ADMUX`, `ADCSRA`)**, telemetri serial dikirim melalui register **USART0 (`UBRR0`, `UCSR0A/B/C`, `UDR0`)**, dan jeda pemulihan (*recovery delay*) selama **5 detik** sebelum membuka jemuran kembali diimplementasikan secara **non-blocking** menggunakan **Overflow Interrupt Timer/Counter2 (`TCCR2A/B`, `TIMSK2`, `ISR(TIMER2_OVF_vect)`)**.

---

## Demo Video Fungsional Sistem

[![Tonton Video Demo di YouTube](https://img.shields.io/badge/YouTube-Tonton%20Demo%20Fungsional%20Sistem-FF0000?style=for-the-badge&logo=youtube&logoColor=white)](https://youtu.be/uGs_xOB5UHw?si=of8Bl_O4KcdK0Zsj)

- **Tautan YouTube:** [https://youtu.be/uGs_xOB5UHw?si=of8Bl_O4KcdK0Zsj](https://youtu.be/uGs_xOB5UHw?si=of8Bl_O4KcdK0Zsj)

---

## Arsitektur Perangkat Keras & Konfigurasi Pin (Hardware Pinout)

```mermaid
flowchart LR
    subgraph Sensors["Input Sensor Lingkungan"]
        LDR["Modul Sensor LDR\n(Cahaya / Gelap)"]
        MQ2["Sensor Gas MQ-2\n(Asap / Polusi)"]
        RAIN["Raindrop Sensor\n(Tetesan Air Hujan)"]
    end

    subgraph MCU["ATmega328P (Arduino Uno R3 @ 16 MHz)"]
        GPIO["Digital Input (Pin D2)"]
        ADC["10-Bit SAR ADC\n(ADMUX & ADCSRA)"]
        TMR2["8-Bit Timer/Counter2\n(ISR TIMER2_OVF_vect)"]
        PWM["PWM Output (Pin D9)"]
        UART["Hardware USART0 TX\n(9600 bps, 8N1)"]
    end

    subgraph Output["Aktuator & Pemantauan"]
        SERVO["Motor Servo SG90\n(0° Buka / 180° Tutup)"]
        MON["Serial Monitor\n(Real-Time Telemetry)"]
    end

    LDR -->|Output D0| GPIO
    MQ2 -->|Analog A0 -> ADC1 (Pin A1)| ADC
    RAIN -->|Analog A0 -> ADC2 (Pin A2)| ADC
    GPIO --> TMR2
    ADC --> TMR2
    TMR2 --> PWM
    PWM --> SERVO
    ADC --> UART
    UART --> MON
```

### Tabel Pemetaan Pin (Pinout & Wiring)
| Komponen Perangkat Keras | Pin Modul | Pin Arduino Uno (ATmega328P) | Mode / Antarmuka | Keterangan Teknis |
| :--- | :---: | :---: | :---: | :--- |
| **Modul Sensor Cahaya (LDR)** | `VCC`, `GND`, `D0` | `5V`, `GND`, **`D2` (`PD2`)** | Digital Input | Logika `0` (`LOW`) = Terang, `1` (`HIGH`) = Gelap/Malam |
| **Sensor Gas & Asap (MQ-2)** | `VCC`, `GND`, `A0` | `5V`, `GND`, **`A1` (`PC1 / ADC1`)** | Analog Input (`ADC1`) | Resolusi 10-bit (`0–1023`), mendeteksi asap/gas pada $\ge 400$ |
| **Sensor Hujan (Raindrop)** | `VCC`, `GND`, `A0` | `5V`, `GND`, **`A2` (`PC2 / ADC2`)** | Analog Input (`ADC2`) | Resolusi 10-bit (`0–1023`), nilai turun $\le 650$ saat terkena air |
| **Motor Servo SG90** | `VCC`, `GND`, `SIG` | `5V`, `GND`, **`D9` (`PB1 / OC1A`)** | PWM Output | Sudut `0°` = Jemuran Terbuka, `180°` = Jemuran Tertutup |
| **Serial Monitor (UART TX)** | — | **`D1` (`PD1 / TXD`)** | Hardware USART0 | Mengirim telemetri status sensor secara *real-time* @ `9600 bps` |

---

## Konfigurasi Register AVR (Bare-Metal Implementation)

### 1. Konfigurasi ADC Manual (`ADMUX` & `ADCSRA`)
Pembacaan nilai analog dari sensor MQ-2 (`ADC1`) dan Raindrop (`ADC2`) dilakukan secara langsung melalui register **Successive Approximation ADC 10-bit**:

```cpp
// Inisialisasi pada setup()
ADMUX = (1 << REFS0); // Referensi tegangan AVcc (5V) dengan kapasitor pada pin AREF
ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1); // Enable ADC, Prescaler = 64

// Fungsi pembacaan kanal ADC (ch = 1 untuk A1, ch = 2 untuk A2)
int readADC(int ch) {
  ADMUX = (1 << REFS0) | (ch & 0x07); // Pilih referensi AVcc dan kanal MUX[2:0]
  ADCSRA |= (1 << ADSC);              // Mulai konversi tunggal (Start Conversion)
  while (ADCSRA & (1 << ADSC));       // Polling hingga bit ADSC kembali ke 0 (selesai)
  return ADC;                         // Kembalikan nilai 10-bit (0 - 1023) dari ADCL:ADCH
}
```
* **Frekuensi Clock ADC:** Dengan frekuensi osilator $f_{\text{osc}} = 16\text{ MHz}$ dan *prescaler* $64$ (`ADPS2 = 1`, `ADPS1 = 1`, `ADPS0 = 0`), frekuensi clock ADC adalah $\frac{16\text{ MHz}}{64} = 250\text{ kHz}$ yang menghasilkan pembacaan stabil dan akurat.

---

### 2. Komunikasi Serial USART0 & Perhitungan Baud Rate (`UBRR0`, `UCSR0A/B/C`)
Pengiriman telemetri ke komputer dilakukan tanpa *library* `Serial` bawaan Arduino, melainkan memanipulasi register perangkat keras **USART0** secara langsung pada kecepatan **9600 bps** (format *8-bit data, no parity, 1 stop bit* / `8N1`).

#### Perhitungan Matematis Register `UBRR0` (Mode Normal Asynchronous)
$$\text{UBRR0} = \frac{f_{\text{osc}}}{16 \times \text{BaudRate}} - 1$$

$$\text{UBRR0} = \frac{16.000.000}{16 \times 9600} - 1 = 104,167 - 1 = 103,167 \approx \mathbf{103}$$

* **Tingkat Kesalahan Baud Rate (*Baud Rate Error*):**
  $$\text{Baud}_{\text{aktual}} = \frac{16.000.000}{16 \times (103 + 1)} = 9615,38\text{ bps} \implies \text{Error} = \left| \frac{9615,38 - 9600}{9600} \right| \times 100\% = \mathbf{0,16\%}$$
  Tingkat *error* `0,16%` berada jauh di bawah batas toleransi maksimum UART ($\pm 2\%$), menjamin transmisi karakter bebas *framing error*.

```cpp
// Inisialisasi UART pada setup()
UBRR0H = 0;                                   // Byte tinggi Baud Rate = 0
UBRR0L = 103;                                 // Byte rendah Baud Rate = 103 (9600 bps @ 16 MHz)
UCSR0B = (1 << TXEN0);                        // Aktifkan pemancar (Transmitter Enable)
UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);       // Format frame: 8-bit data, 1 stop bit

// Pengiriman karakter per byte melalui polling flag UDRE0 (USART Data Register Empty)
void uartPrint(const char *s) {
  while (*s) {
    while (!(UCSR0A & (1 << UDRE0)));         // Tunggu hingga buffer transmit UDR0 kosong
    UDR0 = *s++;                              // Tulis karakter ke register data UART
  }
}
```

---

### 3. Timer/Counter2 Non-Blocking 5-Detik Recovery Delay (`ISR(TIMER2_OVF_vect)`)
Agar mikrokontroler tetap responsif memantau sensor setiap saat tanpa terhenti oleh fungsi `delay()`, penundaan waktu **5 detik** sebelum membuka jemuran kembali (saat cuaca bertransisi dari Bahaya ke Aman) diimplementasikan menggunakan **Overflow Interrupt Timer2 (8-bit)**.

#### Perhitungan Matematis Frekuensi Overflow & Jumlah Tick
Timer2 memiliki lebar pencacah 8-bit ($2^8 = 256\text{ step}$) dan dikonfigurasi dengan **Prescaler 1024** (`CS22 = 1`, `CS21 = 1`, `CS20 = 1`):

$$f_{\text{timer2\_ovf}} = \frac{f_{\text{osc}}}{\text{Prescaler} \times 256} = \frac{16.000.000}{1024 \times 256} = \frac{16.000.000}{262.144} \approx \mathbf{61,035\text{ Hz}}$$

Artinya, rutin interupsi `ISR(TIMER2_OVF_vect)` dieksekusi sebanyak **61,035 kali per detik** (setiap $\approx 16,384\text{ ms}$). Untuk mencapai waktu tunda pemulihan selama **5 detik**, jumlah *tick* yang dibutuhkan adalah:

$$\text{Target Tick} = 5\text{ detik} \times 61,035\text{ Hz} = 305,175 \approx \mathbf{305\text{ tick}}$$

$$t_{\text{delay}} = \frac{305\text{ tick}}{61,035\text{ Hz}} \approx \mathbf{4,997\text{ detik}} \approx \mathbf{5\text{ detik}}$$

```cpp
// Inisialisasi Timer2 pada setup()
TCCR2A = 0;                                           // Mode Normal (non-PWM)
TCCR2B = (1 << CS22) | (1 << CS21) | (1 << CS20);     // Prescaler 1024
TIMSK2 = (1 << TOIE2);                                // Enable Timer2 Overflow Interrupt
sei();                                                // Aktifkan Global Interrupt (SREG I-bit)

// Interrupt Service Routine (dieksekusi otomatis setiap ~16.384 ms)
ISR(TIMER2_OVF_vect) {
  if (timerRun) tick++;
}
```

---

## Tabel Keputusan & Logika Operasional Sistem

Sistem mengevaluasi ekspresi boolean `kondisiAman` secara kontinu di dalam `loop()`:

```cpp
bool kondisiAman = (ldr == 0 && mq2 < 400 && rain > 650);
```

| Status Cahaya (`LDR` Pin D2) | Konsentrasi Gas/Asap (`MQ-2` ADC1) | Intensitas Hujan (`Raindrop` ADC2) | Status Sistem | Respons Timer2 | Posisi Servo SG90 (Pin D9) | Output UART Serial Monitor |
| :---: | :---: | :---: | :---: | :--- | :---: | :--- |
| **Terang** (`0`) | **Bersih** (`< 400`) | **Kering** (`> 650`) | **AMAN** | Mulai menghitung hingga `305 tick` ($\approx 5\text{ s}$) | **Buka (`0°`)** *(setelah stabil 5 detik)* | `Aman L=0 MQ=<val> R=<val>` |
| **Gelap / Malam** (`1`) | *Berapa saja* | *Berapa saja* | **BAHAYA** | Dihentikan & di-reset (`stopTimer()`) | **Tutup Instan (`180°`)** | `Bahaya L=1 MQ=<val> R=<val>` |
| *Berapa saja* | **Ada Asap/Gas** (`>= 400`) | *Berapa saja* | **BAHAYA** | Dihentikan & di-reset (`stopTimer()`) | **Tutup Instan (`180°`)** | `Bahaya L=<0/1> MQ=<val> R=<val>` |
| *Berapa saja* | *Berapa saja* | **Turun Hujan** (`<= 650`) | **BAHAYA** | Dihentikan & di-reset (`stopTimer()`) | **Tutup Instan (`180°`)** | `Bahaya L=<0/1> MQ=<val> R=<val>` |

> [!IMPORTANT]
> **Mekanisme Anti-Oscillation (Histeresis Waktu 5 Detik):**
> Saat terdeteksi kondisi **Bahaya** (hujan turun, muncul asap, atau hari mulai gelap), servo **langsung menutup jemuran (`180°`) seketika tanpa jeda** demi melindungi pakaian. Sebaliknya, ketika kondisi kembali **Aman**, sistem tidak langsung membuka jemuran, melainkan mengaktifkan `Timer2` dan menunggu selama **5 detik (`305 tick`)**. Jika di tengah masa tunggu 5 detik tersebut cuaca kembali memburuk, timer langsung dibatalkan dan di-reset ke nol. Jemuran hanya akan dibuka kembali (`0°`) apabila lingkungan benar-benar stabil aman selama 5 detik berturut-turut.

---

## Struktur Repositori

```text
03-avr-baremetal-smart-clothesline/
├── src/
│   └── JemuranOtomatis_AVR.ino   # Source code utama berbasis register AVR (ATmega328P)
└── README.md                     # Dokumentasi teknis perangkat keras, register, dan perhitungan
```

## Cara Mengunggah ke Arduino Uno
1. Buka file [`src/JemuranOtomatis_AVR.ino`](src/JemuranOtomatis_AVR.ino) menggunakan **Arduino IDE**.
2. Hubungkan papan **Arduino Uno R3 (ATmega328P)** ke komputer melalui kabel USB.
3. Pilih **Board:** `Arduino Uno` dan **Port:** port COM yang sesuai.
4. Klik **Upload**.
5. Buka **Serial Monitor** dan atur *baud rate* ke **`9600 baud`** untuk memantau pembacaan sensor (`L`, `MQ`, `R`) dan status jemuran secara *real-time*.

---

## Tim Pengembang
**Program Studi Teknik Komputer — Fakultas Ilmu Komputer, Universitas Brawijaya**
* **M Dzakir Thuha Al Ghaffar** (245150307111004)
* **Abdillah Abyan Ilman Nafian** (245150300111003)
* **Faaza Fauzan Adzim** (245150307111010)
* **Samuel Fransjemima** (245150300111038)
* **Ridhwan Lathifan Faza** (245150307111038)

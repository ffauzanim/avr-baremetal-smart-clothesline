#include <Servo.h> //library servo
#include <avr/interrupt.h> //library interrupt AVR

Servo s; //objek servo

volatile unsigned long tick = 0; //counter overflow timer
volatile bool timerRun = false; //status timer aktif atau tidak

int readADC(int ch) { //fungsi baca ADC manual
  ADMUX = (1 << REFS0) | (ch & 0x07); //set referensi AVcc dan channel ADC
  ADCSRA |= (1 << ADSC); //mulai konversi ADC
  while (ADCSRA & (1 << ADSC)); //tunggu konversi selesai
  return ADC; //kembalikan nilai ADC
}

void uartPrint(const char *s) { //fungsi kirim string UART manual
  while (*s) {
    while (!(UCSR0A & (1 << UDRE0))); //tunggu buffer kosong
    UDR0 = *s++;
  }
}

void uartPrintNum(int x) { //fungsi kirim angka
  char b[10];
  itoa(x, b, 10); //konversi int ke string
  uartPrint(b);
}

ISR(TIMER2_OVF_vect) { //ISR timer2 overflow
  if (timerRun) tick++; //inkrement jika timer aktif
}

void startTimer() { //mulai timer
  tick = 0;
  timerRun = true;
}

void stopTimer() { //hentikan timer
  timerRun = false;
}

void setup() {
  pinMode(2, INPUT); //input digital LDR

  ADMUX = (1 << REFS0); //referensi ADC AVcc
  ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1); //aktifkan ADC, prescaler 64

  TCCR2A = 0; //mode normal timer2
  TCCR2B = (1 << CS22) | (1 << CS21) | (1 << CS20); //prescaler 1024
  TIMSK2 = (1 << TOIE2); //enable interrupt overflow

  UBRR0H = 0;
  UBRR0L = 103; //baudrate 9600
  UCSR0B = (1 << TXEN0); //enable tx uart
  UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); //format 8 bit data

  sei(); //aktifkan global interrupt

  s.attach(9); //servo pada pin D9 PWM
  s.write(0); //posisi awal buka
}

void loop() {
  int ldr = digitalRead(2); //LDR HIGH = gelap
  int mq2 = readADC(1); //nilai analog MQ-2
  int rain = readADC(2); //nilai analog sensor hujan

  bool kondisiAman = (ldr == 0 && mq2 < 400 && rain > 650); //logika kondisi aman

  if (!kondisiAman) { //jika kondisi bahaya
    stopTimer(); //timer dihentikan
    s.write(180); //servo menutup jemuran

    uartPrint("Bahaya L=");
    uartPrint(ldr ? "1" : "0");
    uartPrint(" MQ=");
    uartPrintNum(mq2);
    uartPrint(" R=");
    uartPrintNum(rain);
    uartPrint("\n");

    return; //keluar loop sehingga tidak masuk ke bagian aman
  }

  if (!timerRun) startTimer(); //jika aman dan timer belum aktif, mulai timer

  if (tick >= 305) { //sekitar 5 detik delay
    stopTimer(); //timer selesai
    s.write(0); //servo buka jemuran
  }

  uartPrint("Aman L=");
  uartPrint(ldr ? "1" : "0");
  uartPrint(" MQ=");
  uartPrintNum(mq2);
  uartPrint(" R=");
  uartPrintNum(rain);
  uartPrint("\n");
}

#include <MK64F12.h> // เรียกใช้ Header File หลักของไมโครคอนโทรลเลอร์ NXP MK64F12
#define MASK(x) (1UL << (x)) // มาโครสำหรับเลื่อนบิต (Bit-shifting) เพื่อกำหนดตำแหน่งขาพินและรีจิสเตอร์
// ส่วนที่ 1: การกำหนดค่าคงที่และตารางข้อมูล (Definitions & Lookup Tables)
// ตารางค่าครึ่งคาบเวลา (Half-Period) ของตัวโน้ตดนตรี หน่วยเป็น Tick (1 Tick = 5 ไมโครวินาที)
// คำนวณจากสูตร: HalfPeriod = 100,000 / ความถี่โน้ต (Hz)
#define NOTE_REST 0    // เงียบ (หยุดเสียง)
#define NOTE_C4   191  // โน้ต โด (262 Hz)
#define NOTE_D4   170  // โน้ต เร (294 Hz)
#define NOTE_E4   152  // โน้ต มี (330 Hz)
#define NOTE_F4   143  // โน้ต ฟา (349 Hz)
#define NOTE_FS4  135  // โน้ต ฟา# (370 Hz)
#define NOTE_G4   128  // โน้ต ซอล (392 Hz)
#define NOTE_A4   114  // โน้ต ลา (440 Hz)
#define NOTE_B4   101  // โน้ต ที (494 Hz)
#define NOTE_C5   96   // โน้ต โดสูง (523 Hz)
#define NOTE_CS5  90   // โน้ต โด#สูง (554 Hz)
#define NOTE_D5   85   // โน้ต เรสูง (587 Hz)
#define NOTE_E5   76   // โน้ต มีสูง (659 Hz)
#define NOTE_G5   64   // โน้ต ซอลสูง (784 Hz)
// โครงสร้างข้อมูล (Struct) สำหรับเก็บข้อมูลตัวโน้ต 1 ตัว (ความถี่ และ ความยาวจังหวะ)
typedef struct {
    uint16_t half_period; // ค่าครึ่งคาบเวลาเพื่อกำหนดความถี่เสียง
    uint8_t  duration;    // ความยาวในการเล่นโน้ตตัวนั้น (1 หน่วย = 0.05 วินาที หรือ 50ms)
} Note;

// อาร์เรย์เก็บทำนองเพลงฉลองแชมป์เมื่อผู้เล่นทำคะแนนครบ 9 แต้ม: "We Are The Champions"
const Note CHAMPION_MELODY[] = {
    // ท่อนที่ 1: "We are the cham - pions, my friends..."
    {NOTE_D5, 5},  {NOTE_REST, 1},   // We
    {NOTE_CS5, 2}, {NOTE_REST, 1},   // are
    {NOTE_D5, 2},  {NOTE_REST, 1},   // the
    {NOTE_CS5, 3}, {NOTE_REST, 1},   // cham-
    {NOTE_A4, 4},  {NOTE_REST, 1},   // pions,
    {NOTE_FS4, 2}, {NOTE_REST, 1},   // my
    {NOTE_B4, 3},  {NOTE_REST, 1},   // friends...
    {NOTE_FS4, 5}, {NOTE_REST, 2},   // จบประโยคแรก

    // ท่อนที่ 2: "And we'll keep on figh - ting till the end..."
    {NOTE_D5, 2},  {NOTE_REST, 1},   // And
    {NOTE_E5, 2},  {NOTE_REST, 1},   // we'll
    {NOTE_G5, 3},  {NOTE_REST, 1},   // keep
    {NOTE_E5, 2},  {NOTE_REST, 1},   // on
    {NOTE_G5, 3},  {NOTE_REST, 1},   // figh-
    {NOTE_E5, 3},  {NOTE_REST, 1},   // ting
    {NOTE_D5, 2},  {NOTE_REST, 1},   // till
    {NOTE_C5, 2},  {NOTE_REST, 1},   // the
    {NOTE_D5, 7},  {NOTE_REST, 1}    // end...
};

// รูปแบบจังหวะเสียง Buzzer แบบเปิด-ปิด (1 = เปิดเสียง, 0 = ปิดเสียง ทุกๆ 0.05 วินาที)
const uint8_t WIN_MELODY[]  = {1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 0}; // เสียงชนะรอบปกติ
const uint8_t FOUL_MELODY[] = {1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 0};             // เสียงแจ้งเตือนกดฟาวล์ก่อนสัญญาณ

// ส่วนที่ 2: ตัวแปรส่วนกลาง (Global Variables)
volatile int score_p1 = 0;        // คะแนนผู้เล่นที่ 1 (0-9) แสดงผลที่ 7-Segment หลักสิบ
volatile int score_p2 = 0;        // คะแนนผู้เล่นที่ 2 (0-9) แสดงผลที่ 7-Segment หลักหน่วย
 
// สถานะของเกม (State Machine): 
// 0 = Idle (รอเริ่มเกม), 1 = Waiting (หน่วงเวลาสุ่ม), 2 = Ready (ไฟติด-พร้อมกด), 3 = Round Over (จบรอบ)
volatile int game_state = 0;      
volatile int wait_ticks = 0;      // ตัวนับถอยหลังช่วงสุ่มรอสัญญาณ (หน่วยละ 0.05 วินาที)
volatile int random_seed = 15;    // ตัวแปรนับค่าต่อเนื่องเพื่อใช้สุ่มเวลาหน่วงในแต่ละรอบ
// โหมดเสียง: 0 = ปิดเสียง, 1 = เล่นจังหวะชนะ/ฟาวล์, 2 = เล่นเพลงฉลองแชมป์ (ครบ 9 แต้ม)
volatile int sound_mode = 0;

// ตัวแปรควบคุมการเล่นเสียงโหมดที่ 1 (WIN_MELODY / FOUL_MELODY)
volatile const uint8_t *current_rhythm = 0; // พอยน์เตอร์ชี้ไปยังรูปแบบจังหวะเสียงที่กำลังเล่น
volatile int rhythm_len = 0;                // ความยาวทั้งหมดของแพทเทิร์นเสียง
volatile int rhythm_idx = 0;                // ตำแหน่งลำดับจังหวะปัจจุบัน

// ตัวแปรควบคุมการเล่นเสียงโหมดที่ 2 (CHAMPION_MELODY)
volatile int melody_len = 0;                // จำนวนตัวโน้ตทั้งหมดในเพลง
volatile int melody_idx = 0;                // ลำดับตัวโน้ตปัจจุบันที่กำลังเล่น
volatile int note_time_left = 0;            // เวลาที่เหลือของตัวโน้ตปัจจุบัน
volatile uint16_t current_half_period = 0;  // ค่าครึ่งคาบเวลาของตัวโน้ตปัจจุบัน
volatile uint16_t tone_counter = 0;         // ตัวนับรอบความถี่เสียงใน Interrupt

// ตัวแปรฐานเวลา (Base Timers)
volatile uint16_t tick_50ms_counter = 0;    // ตัวนับสะสมครบ 10,000 ครั้ง (10,000 * 5us = 50ms)
volatile int buzzer_pin_state = 0;          // เก็บสถานะลอจิกปัจจุบันของขา Buzzer (1 = ดัง, 0 = ดับ)

// ตารางรหัสแสดงผลตัวเลข 0-9 บนจอ 7-Segment แบบ Common Anode (ลอจิก 0 = ติด, 1 = ดับ)
const uint8_t SEGMENT_MAP[10] = {0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8, 0x80, 0x90};
// ตารางเลือกหลักแสดงผลบน 7-Segment (0xF8 = หลักหน่วย/P2, 0xF4 = หลักสิบ/P1)
const uint8_t DIGIT_MAP[2]    = {0xF8, 0xF4};
// ส่วนที่ 3: ฟังก์ชันหน่วงเวลาและควบคุม Buzzer (Helper Functions)
// ฟังก์ชันหน่วงเวลาอย่างง่ายระดับมิลลิวินาที (Software Delay)
void delay_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms * 10000; i++) {
        __NOP(); // คำสั่ง No Operation (ให้ CPU รอ 1 รอบสัญญาณนาฬิกา)
    }
}
 
// ฟังก์ชันเปิดเสียง Buzzer (สั่งขา PTA1 เป็น Output และขับลอจิก LOW 0V เนื่องจากเป็น Active Low)
void buzzer_on(void) {
    PORTA->PCR[1] = (PORTA->PCR[1] & ~PORT_PCR_MUX_MASK) | PORT_PCR_MUX(1); // ตั้งค่าขา PTA1 เป็น GPIO
    PTA->PDDR |= MASK(1);  // กำหนดทิศทางขา PTA1 เป็น Output
    PTA->PCOR = MASK(1);   // จ่ายลอจิก LOW (0V) เพื่อเปิดวงจร Buzzer
    buzzer_pin_state = 1;
}
 

// ฟังก์ชันปิดเสียง Buzzer แบบตัดกระแสสมบูรณ์ (แก้ปัญหาไฟ 5V จาก Shield ไหลย้อนเข้าขา MCU 3.3V)
void buzzer_off(void) {
    PTA->PSOR = MASK(1);   // จ่ายลอจิก HIGH (3.3V)
    PTA->PDDR &= ~MASK(1); // เปลี่ยนขาเป็น Input (High-Impedance) เพื่อตัดกระแสฐานทรานซิสเตอร์บน Shield
    PORTA->PCR[1] = (PORTA->PCR[1] & ~PORT_PCR_MUX_MASK) | PORT_PCR_MUX(0); // ปิดฟังก์ชันพินชั่วคราว
    buzzer_pin_state = 0;
}
// ฟังก์ชันหยุดเสียงทั้งหมดและรีเซ็ตค่าตัวแปรระบบเสียง
void stop_all_sound(void) {
    sound_mode = 0;
    rhythm_len = 0;
    rhythm_idx = 0;
    melody_len = 0;
    melody_idx = 0;
    current_half_period = NOTE_REST;
    buzzer_off();
}
// ส่วนที่ 4: ฟังก์ชันตั้งค่าเริ่มต้นฮาร์ดแวร์ (Initialization Functions)
// ฟังก์ชันตั้งค่าพอร์ต GPIO ทั้งหมด (สวิตช์, หลอด LED, Buzzer และไอซี 74HC595)
void init_GPIO(void) {
    delay_ms(1000); // หน่วงเวลา 1 วินาทีตอนเปิดเครื่อง เพื่อให้วงจรโปรแกรม (SWD/JTAG) เชื่อมต่อได้เสถียร
    // เปิดสัญญาณนาฬิกา (Clock) ให้กับ Port A, B, C และ D
    SIM->SCGC5 |= SIM_SCGC5_PORTA_MASK | SIM_SCGC5_PORTB_MASK | 
                  SIM_SCGC5_PORTC_MASK | SIM_SCGC5_PORTD_MASK;
 
    // ตั้งค่าเริ่มต้นให้ Buzzer (ขา PTA1) อยู่ในสถานะปิดเสียง
    buzzer_off();
 
    // ตั้งค่าสวิตช์กดติดปล่อยดับ 3 ตัว: S1(PTB3 = เริ่มเกม), S2(PTB10 = ผู้เล่น 1), S3(PTB11 = ผู้เล่น 2)
    // เปิดใช้งานตัวต้านทาน Pull-up ภายในชิป (PE_MASK | PS_MASK) เพื่อให้สถานะปกติเป็น HIGH
    PORTB->PCR[3]  = (PORTB->PCR[3]  & ~PORT_PCR_MUX_MASK) | PORT_PCR_MUX(1) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    PORTB->PCR[10] = (PORTB->PCR[10] & ~PORT_PCR_MUX_MASK) | PORT_PCR_MUX(1) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    PORTB->PCR[11] = (PORTB->PCR[11] & ~PORT_PCR_MUX_MASK) | PORT_PCR_MUX(1) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    PTB->PDDR &= ~(MASK(3) | MASK(10) | MASK(11)); // กำหนดทิศทางขาทั้ง 3 เป็น Input
 
    // ตั้งค่าหลอด LED บน Shield 4 ดวง: D4(PTD0), D1(PTD1), D3(PTD2), D2(PTD3)
    PORTD->PCR[0] = (PORTD->PCR[0] & ~PORT_PCR_MUX_MASK) | PORT_PCR_MUX(1);
    PORTD->PCR[1] = (PORTD->PCR[1] & ~PORT_PCR_MUX_MASK) | PORT_PCR_MUX(1);
    PORTD->PCR[2] = (PORTD->PCR[2] & ~PORT_PCR_MUX_MASK) | PORT_PCR_MUX(1);
    PORTD->PCR[3] = (PORTD->PCR[3] & ~PORT_PCR_MUX_MASK) | PORT_PCR_MUX(1);
    PTD->PDDR |= (MASK(0) | MASK(1) | MASK(2) | MASK(3)); // กำหนดทิศทางเป็น Output
    PTD->PSOR  = (MASK(0) | MASK(1) | MASK(2) | MASK(3)); // สั่งดับไฟ LED ทุกดวง (เนื่องจาก LED เป็น Active Low)
    // ตั้งค่าขาควบคุมไอซี Shift Register (74HC595) สำหรับขับจอ 7-Segment:
    // ขา Latch = PTB23, ขา Clock = PTC3, ขา Serial Data = PTC12
    PORTB->PCR[23] = (PORTB->PCR[23] & ~PORT_PCR_MUX_MASK) | PORT_PCR_MUX(1);
    PTB->PDDR |= MASK(23); // กำหนด Latch เป็น Output
 
    PORTC->PCR[3]  = (PORTC->PCR[3]  & ~PORT_PCR_MUX_MASK) | PORT_PCR_MUX(1);
    PORTC->PCR[12] = (PORTC->PCR[12] & ~PORT_PCR_MUX_MASK) | PORT_PCR_MUX(1);
    PTC->PDDR |= (MASK(3) | MASK(12)); // กำหนด Clock และ Data เป็น Output
}

// ฟังก์ชันตั้งค่าโมดูลจับเวลา PIT (Periodic Interrupt Timer)
void init_PIT_Timer(void) {
    SIM->SCGC6 |= SIM_SCGC6_PIT_MASK; // เปิดสัญญาณนาฬิกาให้โมดูล PIT
    PIT->MCR = 0x00;                  // เปิดใช้งานโมดูล PIT (Enable PIT Module)
    // ตั้งค่าให้เกิด Interrupt ทุกๆ 5 ไมโครวินาที (Bus Clock 60MHz * 5us - 1 = 299)
    PIT->CHANNEL[0].LDVAL = 299; 
    PIT->CHANNEL[0].TCTRL = PIT_TCTRL_TIE_MASK | PIT_TCTRL_TEN_MASK; // เปิดใช้งาน Timer และ Interrupt
    NVIC_EnableIRQ(PIT0_IRQn); // อนุญาตการขัดจังหวะ (Enable IRQ) ของ PIT Channel 0 ใน NVIC
}


// ส่วนที่ 5: ฟังก์ชันควบคุมอุปกรณ์ต่อพ่วงและตรรกะเกม (Peripheral & Game Logic)
// ฟังก์ชันส่งข้อมูลอนุกรม 8 บิต เข้าสู่ไอซี 74HC595 ทีละบิต (MSB First)
void shift_out_595(uint8_t data) {
    for (int i = 0; i < 8; i++) {
        if (data & (1 << (7 - i))) {
            PTC->PSOR = MASK(12); // ส่งข้อมูลบิตเป็น 1 (Data = HIGH)
        } else {
            PTC->PCOR = MASK(12); // ส่งข้อมูลบิตเป็น 0 (Data = LOW)
        }
        PTC->PSOR = MASK(3);      // สร้างขอบขาขึ้นของสัญญาณนาฬิกา (Clock = HIGH) เพื่อเลื่อนข้อมูล
        __NOP();
        PTC->PCOR = MASK(3);      // ดึงสัญญาณนาฬิกากลับเป็น 0 (Clock = LOW)
    }
}
// ฟังก์ชันอัปเดตตัวเลขคะแนนบนจอ 7-Segment แบบ Multiplexing (สลับแสดงผลทีละหลักอย่างรวดเร็ว)
void update_7segment(int p1, int p2) {
    // 1. แสดงผลคะแนนผู้เล่นที่ 2 (P2) ที่หลักหน่วย (ขวาสุด)
    PTB->PCOR = MASK(23);             // ดึงขา Latch ลง LOW เพื่อเตรียมรับข้อมูลใหม่
    shift_out_595(SEGMENT_MAP[p2]);   // ส่งรหัสตัวเลขคะแนน P2
    shift_out_595(DIGIT_MAP[0]);      // ส่งรหัสเลือกตำแหน่งหลักหน่วย
    PTB->PSOR = MASK(23);             // ดึงขา Latch ขึ้น HIGH เพื่อส่งข้อมูลออกแสดงผล
    delay_ms(2);                      // ค้างการแสดงผลไว้ 2 มิลลิวินาที
    // 2. แสดงผลคะแนนผู้เล่นที่ 1 (P1) ที่หลักสิบ (ซ้ายมือ)
    PTB->PCOR = MASK(23);             // ดึงขา Latch ลง LOW
    shift_out_595(SEGMENT_MAP[p1]);   // ส่งรหัสตัวเลขคะแนน P1
    shift_out_595(DIGIT_MAP[1]);      // ส่งรหัสเลือกตำแหน่งหลักสิบ
    PTB->PSOR = MASK(23);             // ดึงขา Latch ขึ้น HIGH
    delay_ms(2);                      // ค้างการแสดงผลไว้ 2 มิลลิวินาที
}
// ฟังก์ชันเริ่มต้นเล่นเสียงตามประเภทเหตุการณ์ในเกม
// type = 1 (ชนะรอบปกติ), type = 2 (กดฟาวล์), type = 3 (เพลงฉลองแชมป์ครบ 9 แต้ม)
void start_buzzer_rhythm(int type) {
    stop_all_sound(); // หยุดเสียงเก่าที่ค้างอยู่ก่อนเสมอ
    if (type == 1) {
        current_rhythm = WIN_MELODY;
        rhythm_len = sizeof(WIN_MELODY);
        rhythm_idx = 1;
        sound_mode = 1;
        if (current_rhythm[0] == 1) buzzer_on();
        else buzzer_off();
    } 
    else if (type == 2) {
        current_rhythm = FOUL_MELODY;
        rhythm_len = sizeof(FOUL_MELODY);
        rhythm_idx = 1;
        sound_mode = 1;
        if (current_rhythm[0] == 1) buzzer_on();
        else buzzer_off();
    } 
    else if (type == 3) {
        melody_len = sizeof(CHAMPION_MELODY) / sizeof(Note); // คำนวณจำนวนโน้ตทั้งหมด
        melody_idx = 0;
        current_half_period = CHAMPION_MELODY[0].half_period;
        note_time_left = CHAMPION_MELODY[0].duration;
        tone_counter = 0;
        sound_mode = 2;
    }
}
 
// ฟังก์ชันอ่านค่าการกดสวิตช์และประมวลผลกติกาเกมตามสถานะ (State Machine)
void read_switches(void) {
    // ตรวจสอบปุ่ม S1 (PTB3): ปุ่มเริ่มเกมรอบใหม่ (หรือรีเซ็ตคะแนนเมื่อมีผู้ชนะครบ 9 แต้ม)
    if ((PTB->PDIR & MASK(3)) == 0) {
        if (game_state == 0 || game_state == 3) {
            // หากมีผู้เล่นทำคะแนนครบ 9 แต้มแล้ว ให้รีเซ็ตคะแนนทั้งคู่เป็น 0-0
            if (score_p1 >= 9 || score_p2 >= 9) {
                score_p1 = 0;
                score_p2 = 0;
            }
            // ปิดไฟ LED ทุกดวงและหยุดเสียงทั้งหมดก่อนเริ่มรอบใหม่
            PTD->PSOR = (MASK(0) | MASK(1) | MASK(2) | MASK(3));
            stop_all_sound();
            
            // สุ่มเวลาหน่วงก่อนให้สัญญาณไฟเริ่มเกม ระหว่าง 1.5 - 4.5 วินาที (30 ถึง 90 Ticks)
            wait_ticks = 30 + (random_seed % 61);
            game_state = 1; // เข้าสู่สถานะรอสัญญาณ (Waiting State)
        }
        // หน่วงรอจนกว่าผู้เล่นจะปล่อยปุ่ม S1 พร้อมอัปเดตหน้าจอไม่ให้กระพริบ
        while ((PTB->PDIR & MASK(3)) == 0) {
            update_7segment(score_p1, score_p2);
        }
        delay_ms(50); // ป้องกันสัญญาณกระเพื่อมจากการกดปุ่ม (Switch Debounce)
    }
 
    // กรณีที่ 1: สถานะพร้อมกด (game_state == 2 / สัญญาณไฟ GO! ติดแล้ว)
    if (game_state == 2) {
        // ผู้เล่นที่ 1 (ปุ่ม S2 = PTB10) กดได้เร็วกว่า
        if ((PTB->PDIR & MASK(10)) == 0) {
            game_state = 3; // เปลี่ยนเป็นสถานะจบรอบ
            if (score_p1 < 9) score_p1++;
            PTD->PSOR = (MASK(0) | MASK(1) | MASK(3)); // ปิดไฟ D4, D1, D2
            PTD->PCOR = MASK(2);                       // เปิดไฟ LED D3 แสดงว่า P1 ชนะในรอบนี้
            
            if (score_p1 >= 9) start_buzzer_rhythm(3); // ครบ 9 แต้ม เล่นเพลงฉลองแชมป์
            else start_buzzer_rhythm(1);               // ชนะรอบปกติ เล่นเสียงชนะ
        }
        // ผู้เล่นที่ 2 (ปุ่ม S3 = PTB11) กดได้เร็วกว่า
        else if ((PTB->PDIR & MASK(11)) == 0) {
            game_state = 3; // เปลี่ยนเป็นสถานะจบรอบ
            if (score_p2 < 9) score_p2++;
            PTD->PSOR = (MASK(1) | MASK(2) | MASK(3)); // ปิดไฟ D1, D3, D2
            PTD->PCOR = MASK(0);                       // เปิดไฟ LED D4 แสดงว่า P2 ชนะในรอบนี้
            
            if (score_p2 >= 9) start_buzzer_rhythm(3); // ครบ 9 แต้ม เล่นเพลงฉลองแชมป์
            else start_buzzer_rhythm(1);               // ชนะรอบปกติ เล่นเสียงชนะ
        }
    }



    // กรณีที่ 2: สถานะรอสัญญาณ (game_state == 1) -> กติกากดฟาวล์ (ชิงกดก่อนไฟสัญญาณติด)
    else if (game_state == 1) {
        // ผู้เล่นที่ 1 กดก่อนสัญญาณ -> ปรับฟาวล์ ยกแต้มให้ผู้เล่นที่ 2 ทันที
        if ((PTB->PDIR & MASK(10)) == 0) {
            game_state = 3;
            if (score_p2 < 9) score_p2++;
            PTD->PSOR = (MASK(1) | MASK(2) | MASK(3));
            PTD->PCOR = MASK(0);      // เปิดไฟ LED D4 (P2 ได้แต้มจากการฟาวล์ของ P1)
            
            if (score_p2 >= 9) start_buzzer_rhythm(3); // ครบ 9 แต้ม เล่นเพลงฉลองแชมป์
            else start_buzzer_rhythm(2);               // เล่นเสียงแจ้งเตือนฟาวล์
        }
        // ผู้เล่นที่ 2 กดก่อนสัญญาณ -> ปรับฟาวล์ ยกแต้มให้ผู้เล่นที่ 1 ทันที
        else if ((PTB->PDIR & MASK(11)) == 0) {
            game_state = 3;
            if (score_p1 < 9) score_p1++;
            PTD->PSOR = (MASK(0) | MASK(1) | MASK(3));
            PTD->PCOR = MASK(2);      // เปิดไฟ LED D3 (P1 ได้แต้มจากการฟาวล์ของ P2)
            
            if (score_p1 >= 9) start_buzzer_rhythm(3); // ครบ 9 แต้ม เล่นเพลงฉลองแชมป์
            else start_buzzer_rhythm(2);               // เล่นเสียงแจ้งเตือนฟาวล์
        }
    }
}
 
// ส่วนที่ 6: รูทีนบริการขัดจังหวะ (Interrupt Service Routine - ISR) ทำงานทุก 5us
void PIT0_IRQHandler(void) {
    // 1. สร้างความถี่เสียงดนตรี (Square Wave Generator) สำหรับเพลง "We Are The Champions" (Mode 2)
    if (sound_mode == 2) {
        if (current_half_period > 0) {
            tone_counter++;
            // เมื่อนับครบครึ่งคาบเวลา ให้สลับสถานะขา Buzzer (0 <-> 1) เพื่อสร้างคลื่นความถี่เสียงตามโน้ต
            if (tone_counter >= current_half_period) {
                tone_counter = 0;
                if (buzzer_pin_state == 1) {
                    buzzer_off();
                } else {
                    buzzer_on();
                }
            }
        } else if (buzzer_pin_state == 1) {
            buzzer_off(); // กรณีเป็นโน้ตหยุด (NOTE_REST) ให้ปิดเสียง
        }
    }

    // 2. ตัวหารความถี่สร้างฐานเวลา 50 มิลลิวินาที (10,000 รอบ * 5us = 50ms หรือ 0.05 วินาที)
    tick_50ms_counter++;
    if (tick_50ms_counter >= 10000) {
        tick_50ms_counter = 0;
        random_seed++; // เพิ่มค่าตัวแปรสุ่มตลอดเวลา

        // จัดการจังหวะเสียงโหมดที่ 1 (เสียงชนะปกติ / เสียงฟาวล์)
        if (sound_mode == 1) {
            if (rhythm_idx < rhythm_len) {
                if (current_rhythm[rhythm_idx] == 1) {
                    buzzer_on();
                } else {
                    buzzer_off();
                }
                rhythm_idx++;
            } else {
                stop_all_sound(); // เล่นครบแพทเทิร์นแล้วให้หยุดเสียง
            }
        }

        // จัดการการเปลี่ยนตัวโน้ตของเพลงฉลองแชมป์ (โหมดที่ 2)
        else if (sound_mode == 2) {
            if (note_time_left > 0) {
                note_time_left--; // ลดเวลาคงเหลือของตัวโน้ตปัจจุบัน
            }
            if (note_time_left == 0) {
                melody_idx++; // เลื่อนไปยังตัวโน้ตถัดไป
                if (melody_idx < melody_len) {
                    current_half_period = CHAMPION_MELODY[melody_idx].half_period;
                    note_time_left = CHAMPION_MELODY[melody_idx].duration;
                    tone_counter = 0;
                } else {
                    stop_all_sound(); // เล่นจบเพลงแล้วให้หยุดเสียง
                }
            }
        }

        // จัดการการนับถอยหลังในสถานะรอสัญญาณ (Waiting State)
        if (game_state == 1) {
            if (wait_ticks > 0) {
                wait_ticks--;
                // สั่งให้ไฟ LED D1 และ D2 กระพริบพร้อมกันทุกๆ 0.2 วินาที (4 * 50ms) เพื่อเตือนให้เตรียมพร้อม
                if (wait_ticks % 4 == 0) {
                    PTD->PTOR = (MASK(1) | MASK(3)); // สลับสถานะ (Toggle) หลอด D1 และ D2
                }
            } else {
                // หมดเวลาหน่วงสุ่ม -> เข้าสู่สถานะพร้อมกด (GO!)
                game_state = 2;
                PTD->PCOR = (MASK(1) | MASK(3)); // เปิดไฟ LED D1 และ D2 ค้างไว้เป็นสัญญาณให้กดปุ่ม
            }
        }
    }
 
    // ล้างค่าแฟล็กการขัดจังหวะ (Clear Interrupt Flag) เพื่อพร้อมรับการทำงานรอบถัดไป
    PIT->CHANNEL[0].TFLG = PIT_TFLG_TIF_MASK;
}
// ส่วนที่ 7: ฟังก์ชันหลักของโปรแกรม (Main Function)
int main(void) {
    init_GPIO();      // เรียกใช้ฟังก์ชันตั้งค่าพอร์ต GPIO ทั้งหมด
    init_PIT_Timer(); // เรียกใช้ฟังก์ชันตั้งค่า Timer Interrupt 5us
 

    // วนลูปทำงานหลักอย่างต่อเนื่อง (Super Loop)
    while (1) {
        read_switches();                    // ตรวจสอบการกดปุ่มและการตัดสินผลเกม
        update_7segment(score_p1, score_p2); // อัปเดตการแสดงผลคะแนนบนจอ 7-Segment ตลอดเวลา
    }
}

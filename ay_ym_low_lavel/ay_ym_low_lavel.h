#pragma once

#include "module_shift_register.h"
#include "mc_hardware_interfaces_timer.h"
#include "mc_hardware_interfaces_pin.h"
#include "user_os.h"
#include "string.h"

// Данная структура показывает, как подключен чип AY к сдвиговому регистру.
// На каждый чип должна быть своя такая структура.

// Пример использования:
// При "прямом подключении": { 0, 1, 2, 3, 4, 5, 6, 7 }.
// При "обратном": { 7, 6, 5, 4, 3, 2, 1 }
struct ay_ym_connection_chip_cfg_t {
    const uint8_t   bias_bit[8];
};

/*
 * Структура описывает формат подключения AY/YM чипов к микроконтроллеру и
 * предоставляет низкоуровневые функции работы с ними.
 *
 * Предполагается, что к микроконтроллеру подключен модуль shift_register,
 * представляющий из себя сборку последовательно соединенных сдвиговых регистров.
 */
struct ay_ym_low_lavel_cfg_t {
    const module_shift_register*    const sr;               /*
                                                             * Объект расширителя портов должен быть заранее сконфигурирован на нужное колличество
                                                             * AY/YM. По 1-му байту на штуку.
                                                             * AY/YM должны быть подключены на сдвиговые регистры, подключенные последовательно
                                                             * друг к другу.
                                                             * Например, на 0-й и 1-й байт. 4, 5-й. Но никак не 0 и 2. Разрывов быть не должно.
                                                             */
    // Мутекс заберается на время передачи данных в AY/YM через sr.
    // Возвращается по окончании цельного пакета. После этого шина
    // spi может быть свободно использована.
    USER_OS_STATIC_MUTEX*           const mutex;
    uint8_t*                        const p_sr_data;        /*
                                                             * Указатель на первый используемый байт, подключенный к AY/YM.
                                                             * В сборке расширителя портов может оказаться несколько сдвиговых регистров
                                                             * (например 5), причем 1-3 используется другими модулями, а 4-5 2-мя AY и YM.
                                                             * В таком случае следует передать указатель на байт, являющимся 4-м байтом сдвигового регистра.
                                                             * Массив для сдвиговых регистров должен быть создан заранее и, в идеале, быть глобальным.
                                                             */

    USER_OS_STATIC_BIN_SEMAPHORE    *const semaphore_sec_out;// Этим симафором будем показывать, что прошла секунда воспроизведения. Опционально (можно nullptr).

    // Выводы управления AY должны быть указаны обязательно (включение всех AY - параллельное).
    // Выводы должны быть указано явно.
    const pin_base*                 const bdir;              // Выводы управления AY чипами (чипы включать в параллель).
    const pin_base*                 const bc1;

    //
    // Для каждого AY своя очередь.
    // Здесь указатель на массив указателей на эти очереди.
    // Очередь должна содержать ay_low_out_data элементы!!!
    //
	USER_OS_STATIC_QUEUE*           queue_array;
    const uint8_t                   ay_number;                                  // Колличество AY на сдвиговом регистре.
    const ay_ym_connection_chip_cfg_t* con_cfg;                                 // Способ подключения каждого чипа.
    const uint8_t                   task_prio;                                  // Приоритет задачи-обработчика данных из очереди.

    tim_comp_one_channel_base*      const tim_frequency_ay;                     // Таймер, который генерирует необходимую частоту для генерации сигнала чипов (соединение в параллель) (~1.75 МГц по-умолчанию).
                                                                                // Должен быть заранее инициализирован.
    tim_interrupt_base*             const tim_interrupt_task;                   // Таймер, который генерирует прерывания для ay_low_lavel.
    void    ( *pwr_set )            ( bool state );                             // false - выключить. true - включить. Для всех чипов и усилителя.
};


struct ay_low_out_data_struct {
    uint8_t     reg;        // Если сюда положат 0xFF, значит, что нужно больше для конкретного чипа в этом интервале времени посылок нет!
    uint8_t     data;
};

/*
 * Очередь элементов для выдачи в AY.
 */
struct ay_queue_struct {
    uint8_t     number_chip;
    uint8_t     reg;        // Когда здесь 0xFF - поле data игнорируется, считается командой паузы в одно прерывание!
    uint8_t     data;
};

struct __attribute__( ( packed ) ) chip_reg {
    uint8_t reg[16];
};

#define AY_YM_LOW_LAVEL_TASK_STACK_SIZE             1000

// Очередь должна быть как минимум 1 элемент (в идеале - по 16*2 b и более для каждого чипа).
// Очердь общая для все чипов!
class ay_ym_low_lavel {
public:
    ay_ym_low_lavel ( const ay_ym_low_lavel_cfg_t* const cfg );

    // Добавляет элемент в очередь. Элемент будет выдан в АУ во время прерывания.
    void queue_add_element       ( ay_queue_struct* data ) const;

    // Метод для приостановки воспроизвдеения и последущего возобновления с того же места.
    void play_state_set          ( uint8_t state ) const;

    // Данный handler должен
    // быть вызван в прерывании по прохождении 50 мс
    // (частота может быть изменена другими методами, в зависимости от конфигурации воспроизведения).
    void timer_interrupt_handler ( void ) const;

    //**********************************************************************
    // Метод возвращает текущее состояние очередей (всех).
    // Возвраащемое значение:
    // true     - все очереди пусты.
    // false    - хотя бы в одной (в случае нескольких AY) есть данные.
    //**********************************************************************
    bool queue_empty_check ( void );

    void hardware_clear ( void ) const;     // Очищает все чипы (начальными значениями).
    void queue_clear ( void ) const;

private:
    const ay_ym_low_lavel_cfg_t* const cfg;

    void out_reg    ( void ) const;
    void out_data   ( void ) const;

    // Метод преобразует байт (который был бы валидным для подключения
    // 0-й бит сдвигового регистра = 0-му биту шины AY. В валидный вид с учетом
    // реального подключения (например, 0-й бит сдвигового регисра = 7-му биту
    // шины данных AY... На тот случай, когда подключение производилось не соответсвующими
    // линиями.
    uint8_t connection_transformation ( const uint8_t chip, const uint8_t& data ) const;

    void send_buffer ( void ) const;

    static void task ( void* p_this );

    // Сбрасывает флаги "паузы" на чипе.
    void reset_flag_wait ( bool* flag_array );
    bool chack_flag_wait ( bool* flag_array );

    /*
     * Этим симафором будем показывать, что пора передать следущую порцию данных.
     * Мы ждем его в задаче ay_queue_out_task и отдаем в ay_timer_handler.
     */
    USER_OS_STATIC_BIN_SEMAPHORE_BUFFER semaphore_buf;
    USER_OS_STATIC_BIN_SEMAPHORE        semaphore     = nullptr;

    /*
     * Для создания задачи.
     */
    USER_OS_STATIC_STACK_TYPE           task_stack[ AY_YM_LOW_LAVEL_TASK_STACK_SIZE ] = { 0 };
    USER_OS_STATIC_TASK_STRUCT_TYPE     task_struct;

    uint8_t tic_ff      = 0;                         // Считаем время воспроизведения (колличество прерываний).

    chip_reg*                           buf_data_chip = nullptr;                        // Массив структур данных регистров.
};

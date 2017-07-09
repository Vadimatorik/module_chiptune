#pragma once

#include "errno.h"
#include "stm32_f20x_f21x_include_module_lib.h"
#include "module_shift_register.h"
#include "string.h"

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
    uint8_t*                        const p_sr_data;        /*
                                                             * Указатель на первый используемый байт, подключенный к AY/YM.
                                                             * В сборке расширителя портов может оказаться несколько сдвиговых регистров
                                                             * (например 5), причем 1-3 используется другими модулями, а 4-5 2-мя AY и YM.
                                                             * В таком случае следует передать указатель на байт, являющимся 4-м байтом сдвигового регистра.
                                                             * Массив для сдвиговых регистров должен быть создан заранее и, в идеале, быть глобальным.
                                                             */
    USER_OS_STATIC_BIN_SEMAPHORE    *const semaphore_sec_out;// Этим симафором будем показывать, что прошла секунда воспроизведения. Опционально (можно NULL). Если не указан, просто игнорируется.
    // Выводы управления AY должны быть указаны обязательно (включение всех AY - параллельное).
    // Выводы должны быть указано явно.
    const pin*                      const bdir;              // Выводы управления AY чипами (чипы включать в параллель).
    const pin*                      const bc1;

    /*
     * Для каждого AY своя очередь.
     * Здесь указатель на массив указателей на эти очереди.
     * Очередь должна содержать ay_low_out_data элементы!!!
     */
    USER_OS_STATIC_QUEUE*           const p_queue_array;
    const uint8_t                   ay_number;              // Колличество AY на сдвиговом регистре.
    const uint8_t                   task_prio;              // Приоритет задачи-обработчика данных из очереди.

    uint8_t*                        const  r7_reg;          // Текущее состояние управляющего регистра r7 каждого чипа (массив по количеству чипов).
};


struct ay_low_out_data {
    uint8_t     reg;    // Если сюда положат 0xFF, значит, что нужно больше для конкретного чипа в этом интервале времени посылок нет!
    uint8_t     data;
};
//int                    *tim_frequency_ay_fd;                // FD таймера, который генерирует необходимую частоту для генерации сигнала (~1.75 МГц по-умолчанию).
//    int                    *tim_event_ay_fd;                    // FD таймера, вызывающего прерывания (лля вывода данных из очереди в AY/YM).

/*
 * Очередь элементов для выдачи в AY.
 */
struct ay_queue_struct {
    uint8_t     number_chip;
    uint8_t     reg;
    uint8_t     data;
};

// Очередь должна быть как минимум 1 элемент (в идеале - по 16*2 b и более для каждого чипа).
// Очердь общая для все чипов!
class ay_ym_low_lavel {
public:
    constexpr ay_ym_low_lavel ( const ay_ym_low_lavel_cfg_t* const cfg ) : cfg(cfg)  {}
    void init ( void ) const;

private:
    const ay_ym_low_lavel_cfg_t* const cfg;

    void out_reg    ( void ) const;
    void out_data   ( void ) const;

    // Данный handler с fd ранее созданного объекта должен быть вызван в прерывании по переполнению таймера, генерирующего прерывания раз в 50 мс (частота может быть изменена другими методами, но по-умолчанию 50 мс).
    void timer_interrupt_handler ( void ) const;
    void queue_add_element       ( ay_queue_struct* data ) const;

    // Включить/выключить 1 канал одного из чипов. Через очередь.
    void set_channel             ( uint8_t number_ay, uint8_t channel, bool set ) const;

    void hardware_clear ( void ) const;
    static void task ( void* p_this );


    /*
     * Этим симафором будем показывать, что пора передать следущую порцию данных.
     * Мы ждем его в задаче ay_queue_out_task и отдаем в ay_timer_handler.
     */
    mutable USER_OS_STATIC_BIN_SEMAPHORE_BUFFER semaphore_buf = USER_OS_STATIC_BIN_SEMAPHORE_BUFFER_INIT_VALUE;
    mutable USER_OS_STATIC_BIN_SEMAPHORE        semaphore     = nullptr;

    /*
     * Для создания задачи.
     */
    mutable USER_OS_STATIC_STACK_TYPE           task_stack[300] = { 0 };
    mutable USER_OS_STATIC_TASK_STRUCT_TYPE     task_struct   = USER_OS_STATIC_TASK_STRUCT_INIT_VALUE;

    /*
     * Далее все сделано так, чтобы можно было поддерживать до 32 AY/YM чипов.
     */

    uint8_t tic_ff      = 0;                         // Считаем время воспроизведения (колличество прерываний).
};

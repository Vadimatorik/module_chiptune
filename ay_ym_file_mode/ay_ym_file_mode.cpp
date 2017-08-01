#include "ay_ym_file_mode.h"

ay_ym_file_mode::ay_ym_file_mode ( ay_ym_file_mode_struct_cfg_t* cfg ) : cfg( cfg ) {
    // Для отдачи команд задачи обновления кольцевого буффера.
    this->queue_update        = USER_OS_STATIC_QUEUE_CREATE( 1, sizeof( uint8_t ), this->queue_update_buf, &this->queue_update_st );
    // Задача будет ставить симафор всякий раз, как часть кольцевого буффера была обнавлена.
    this->c_buf_semaphore     = USER_OS_STATIC_BIN_SEMAPHORE_CREATE( &this->c_buf_semaphore_buf );
    // Задача обновления кольцевого буффера.
    USER_OS_STATIC_TASK_CREATE( this->buf_update_task, "ay_file", 300, ( void* )this, this->cfg->circular_buffer_task_prio, this->task_stack, &this->task_struct );
}

void ay_ym_file_mode::buf_update_task ( void* p_obj ) {
    (void)p_obj;
    while ( true ) {

    }
}

/*
// Принимаем указатели на заранее заданные строки пути и имени.
void file_update (int fd, char *dir, char *name){
    ay_file_mode_cfg_t *ay = eflib_getInstanceByFd (fd);
    if (dir != NULL) ay->directory_path = dir;
    if (name != NULL) ay->file_name = name;
}

// Открываем карту и копируем нужный кусок (1 кусок - number_sector * 512 байт, счет с 0).
int _psg_cpy_array (ay_file_mode_cfg_t *ay, uint32_t sektor, uint16_t point_buffer, uint8_t number_sector, UINT *l){
    xSemaphoreTake ( *ay->cfg->microsd_mutex, (TickType_t) portMAX_DELAY ); // Ждем, пока освободится microsd.
    if (f_lseek(&ay->fil_psg, sektor*512) == FR_OK){	// Переходим к сектору в файле.
        if (f_read(&ay->fil_psg, &ay->psg_file_buf[point_buffer], 512*number_sector, l) == FR_OK) {
            xSemaphoreGive(*ay->cfg->microsd_mutex);		// Показываем, что карта нам теперь не нужна.
            return 0;
        };
    };
    return READ_FILE_ERROR;
}
*/
/* При появлении в очереди элемента - обновляем данные.
 * Имя директории и файла кладется в ay_file_mode_cfg_t заранее.
 * Файл должен быть открыт заранее и закрыт после чтения последнего блока.
 * Предполагается, что все готова для чтения.
 */
/*
void buf_update_task (ay_file_mode_cfg_t *ay){
    uint8_t buffer_queue;
    uint8_t offset = (ay->cfg->circular_buffer_size/2)/512;		// Сколько секторов за 1 раз следует считать. Зависит от буффера.
    while(1){
        xQueueReceive(ay->queue_update_buf, &buffer_queue, (TickType_t) portMAX_DELAY);
        if (buffer_queue == 0){											// Решаем, какую часть буффера сейчас перезаполняем.
            if (_psg_cpy_array(ay, ay->sektor, 0, offset, &ay->l) != 0)
                break;	// Если не считалось (проблемы с картой или еще что - выходим с ошибкой. Все манипуляции с картой внутри метода.
        } else {
            if (_psg_cpy_array(ay, ay->sektor, ay->cfg->circular_buffer_size/2, offset, &ay->l) != 0)
                break;	// Идем с середины.
        }
        ay->sektor += (ay->cfg->circular_buffer_size/2)/512;	// В следущий раз - другой блок.
        xSemaphoreGive(ay->buffer_semaphore);					// Буффером можно пользоваться.
    }
}


// Очищаем AY через очередь.
void ay_clear_to_queue (int fd){
    (void) fd;
    uint16_t buf;
    buf = (7<<8)|0b111111;			// Отключаем все каналы и шумы.
    ay_queue_add_element(fd, &buf);
    for (int l = 0; l<7; l++){		// Очищаем первые 7 регистров.
        buf = l<<8;
        ay_queue_add_element(fd, &buf);
    }
    for (int l = 8; l<16; l++){		// Остальные.
        buf = l<<8;
        ay_queue_add_element(fd, &buf);
    }

}
*/
/* Получаем длину файла PSG в "колличестве 0xFF". По сути, 1 0xFF = 20 мс.
 * Т.к. между ними данные передаются практически мгновенно.
 * Мы должны заранее находится в нужной директории.
 * Передаем указатель на строку-имя файла.
 * Можно напрямую из file_info. Чтобы не тратить время на копирование.
 * ВАЖНО!: метод не следит за mutex-ом MicroSD! => вызываеться данный метод может только из под другого.
 * Который разрулит все проблемы.
 * ВАЖНО!: Т.к. метод дочерний, то указатель на буффер ему тоже нужно передать. Причем там должно быть 512 байт.
 * Как вариант - на момент создания списка - использовать кольцевой буффер.
 *//*
int _ay_psg_file_get_long (int fd, char *name, uint8_t *buffer){
    ay_file_mode_cfg_t *ay = eflib_getInstanceByFd (fd);

    FIL fil_psg;				// Файл, который будем сканировать.
    uint8_t flag_one_read = 0; 	// Флаг первого чтения. Чтобы сразу перескачить заголовок.
    uint32_t file_psg_long = 0;	// Длина файла в 0xFF-ах.
    UINT l = 0;						// Ею будем отслеживать опустошение буффера. К тому же, в ней после считывания хранится число реально скопированныйх байт.
    uint16_t point_data_buf = 16;	// А это номер элемента в буффере, который мы будем тестировать.

    // Если открыть не удалось - значит либо файла не сущетсвует, либо еще чего. Но мы возвращаем, что файл поврежден.
    if (f_open(&fil_psg, name, FA_OPEN_EXISTING | FA_READ ) != FR_OK){
        return OPEN_FILE_ERROR;
    };

    // Идем по блокам файла.
    for (uint32_t loop_byte_file = 16; loop_byte_file < f_size(&fil_psg); loop_byte_file++){
        if (l == 0){	// Если байты закончались - считываем еще 512.
            if (f_read(&fil_psg, buffer, 512, &l) != FR_OK) {		// Если считать не удалось - выходим с ошибкой.
                return READ_FILE_ERROR;
            };
            if (flag_one_read != 0) {
                point_data_buf = 0;
            } else {
                point_data_buf = 16;// Пропускаем заголовок, по-этому, сразу отнимаем.
                l-=16;
                flag_one_read = 1;
            }
        }
        if ((uint8_t)ay->psg_file_buf[point_data_buf] == 0xFF) {
            file_psg_long++;	// Если нашли 0xFF - то это пауза. => 20 мс.
        }
        point_data_buf++;
        l--;
    };

    f_close(&fil_psg);			// Закрываем файл.
    return file_psg_long;
}

// Останавливаем трек и чистим буффера.
void ay_psg_file_play_break(int ay_file_mode_fd){
    ay_file_mode_cfg_t *ay = eflib_getInstanceByFd (ay_file_mode_fd);
    ay->emergency_team = 1; // ay_psg_file_play_from_microSD сканирует эту переменную.
}

// Открываем файл с выбранным именем и воспроизводим его.
int ay_psg_file_play_from_microSD(int fd){
    ay_file_mode_cfg_t *ay = eflib_getInstanceByFd (fd);
    uint16_t buffer_queue = 0;	// Буффер для элемента, который положем в очередь.
    uint8_t flag = 0; 	// Чтобы различать, что мы считали. Регистр - или значение. Сначала - регистр.
    uint16_t point_data_buf = 16;	// А это номер элемента в буффере, из которого мы будем выдавать данные. Начинаем с 16-го файла, т.к. до него у нас заголовок.
    uint8_t command_buffer;			// Сюда будем класть поручение, в какую часть буффера класть данные.
    ay->sektor = 0;								// Читаем с начала.
    ay->emergency_team = 0;						// На случай, если тыкнули на остановку до воспроизведения.
    ay_play_stait(*ay->cfg->fd_ay_hardware, 1);
    ay_clear_to_queue(*ay->cfg->fd_ay_hardware);	// Обязательно стираем настройки старой мелодии. Чтобы звук по началу не был говном.
    // Если открыть не удалось - значит либо файла не сущетсвует, либо еще чего. Но мы возвращаем, что файл поврежден.
    if (f_open(&ay->fil_psg, ay->file_name, FA_OPEN_EXISTING | FA_READ ) != FR_OK){		// Открываем файл, из которого будем читать.
        return OPEN_FILE_ERROR;
    };
    // Заполняем кольцевой буффер.
    xSemaphoreTake(ay->buffer_semaphore, (TickType_t) 0);	// Буффер может быть заполнен другим файлом.
    command_buffer = 0; xQueueSend( ay->queue_update_buf, &command_buffer, portMAX_DELAY  );
    xSemaphoreTake(ay->buffer_semaphore, (TickType_t) portMAX_DELAY);	// Изначально буффер microsd свободен.
    command_buffer = 1; xQueueSend( ay->queue_update_buf, &command_buffer, portMAX_DELAY  );
    for (uint32_t loop_byte_file = 16; loop_byte_file < f_size(&ay->fil_psg); loop_byte_file++, point_data_buf++){
        if (ay->emergency_team != 0){			// Если пришла какая-то срочная команда!
            if (ay->emergency_team == 1){		// Если нужно остановить воспроизведение.
                ay->emergency_team = 0;			// Мы приняли задачу.
                command_buffer = END_TRACK; xQueueSend( *ay->cfg->queue_feedback, &command_buffer, portMAX_DELAY  ); // Сообщаем, что трек закончен.
                return 0;							// Выключаем AY, выдаем в очередь флаг окончания и выходим.
            }
        };
        // Смотрим, не закончилась ли часть буффера.
        if (point_data_buf == ay->cfg->circular_buffer_size/2){
            xSemaphoreTake ( ay->buffer_semaphore, (TickType_t) portMAX_DELAY );	// К этому времени у нас уже должена была перезаписаться часть буффера.
            command_buffer = 0;
            xQueueSend( ay->queue_update_buf, ( void * ) &command_buffer, portMAX_DELAY  );	// Приказываем перезаписать часть, которую уже выдали.
        };
        if (point_data_buf == ay->cfg->circular_buffer_size){
            xSemaphoreTake ( ay->buffer_semaphore, (TickType_t) portMAX_DELAY );	// К этому времени у нас уже должена была перезаписаться часть буффера.
            command_buffer = 1;
            xQueueSend( ay->queue_update_buf, ( void * ) &command_buffer, portMAX_DELAY  );	// Приказываем перезаписать часть, которую уже выдали.
            point_data_buf = 0;
        };
        if ((uint8_t)ay->psg_file_buf[point_data_buf] == 0xFF) {	// 0xFF - простая задержка на ~20 мс. Очередь сама разберется, как с ней быть.
            buffer_queue = 0xFF;
            //ay_queue_add_element(*ay->cfg->fd_ay_hardware, &buffer_queue);
        } else {
            if (flag == 0) {
                buffer_queue = (uint8_t)ay->psg_file_buf[point_data_buf] << 8;
                flag = 1;
            } else {
                buffer_queue |= (uint8_t)ay->psg_file_buf[point_data_buf];
                //ay_queue_add_element(*ay->cfg->fd_ay_hardware, &buffer_queue);
                flag = 0;
            };
        };
    };
    if (f_close(&ay->fil_psg) != FR_OK){		// Все прочитано, закрываем файл.
        //return OPEN_FILE_ERROR; Обработка исключительной ситуации.
    };
    ay_play_stait(*ay->cfg->fd_ay_hardware, 1);	// Очищаем чип (многие psg в конце чип оставляют заполненным).
    ay_delay_clean(*ay->cfg->fd_ay_hardware);	// Ждем, пока все данные в AY передадутся.
    ay_play_stait(*ay->cfg->fd_ay_hardware, 0);
    command_buffer = END_TRACK; xQueueSend( *ay->cfg->queue_feedback, &command_buffer, portMAX_DELAY  ); // Сообщаем, что трек закончен.
    return 0;
}

// Смотрим на разрешение после точки. Если это psg = 1, нет = 0.
int _chack_psg_file (char *point_dot){
    if (((point_dot[1] == 'p')|(point_dot[1] == 'P')) &&
        ((point_dot[2] == 's')|(point_dot[2] == 'S')) &&
        ((point_dot[3] == 'g')|(point_dot[3] == 'G')) &&
        (point_dot[4] == 0)){			// В конце обязательно должен быть 0! Защита от мусора.
        return 1;	// Это psg.
    };
    return 0;	// Не psg.
}
*/
/* Сканируе дерикторию на наличие psg файлов (возвращаем колличетсов ВАЛИДНЫХ файлов).
 * Составляем список psg файлов.
 * 		При этом, если файл битый и его длительность нельзя определить - его в список не включаем.
 *		Формат psg_list.list:
 *		|<--256_символов_имени-->|<-4_байта_uint32_t_время_в_секундах->|
 */
/*
int ay_find_psg_file (int fd){
    // После поиска все структуру работы с FatFS, объявленные здесь - будут удалены за ненадобностью.
    DIR dir_psg_find;				// Директория, которую сканируем.
    FIL fil_psg_list;				// Для записи в list_psg.
    FILINFO fil_info;				// Сюда будут класться имена файлов в директории.
    int result = OPEN_FILE_OK;		// Флаг ошибки (если -n) или колличество psg (если 0+).
    int psg_file_loop = 0;			// Колличество PSG файлов.
    UINT rw_res = 0;				// Для проверки валидности чтения/записи.

    ay_file_mode_cfg_t *ay = eflib_getInstanceByFd (fd);			// Достаем нашу основную структуру.

    xSemaphoreTake ( *ay->cfg->microsd_mutex, (TickType_t) portMAX_DELAY ); // Ждем, пока освободится microsd.

    result = f_opendir(&dir_psg_find, ay->directory_path);
    if (result != FR_OK) return OPEN_DIR_ERROR;
    result = f_open(&fil_psg_list, "psg_list.list", FA_CREATE_ALWAYS | FA_READ | FA_WRITE);
    if (result != FR_OK) return OPEN_FILE_ERROR;

    // Если все открылось - идем дальше.
    memset(ay->psg_file_buf, 0, ay->cfg->circular_buffer_size);	// Чистим массив (Чтобы все было четко потом. Без этого нельзя.)

    // Перебираем все файлы.
    while (1){
        memset(&fil_info, 0, sizeof(fil_info));		// Чистим имя файла, т.к. новый может оказаться меньше старого, и тогда после символа окончания строки будет еще мусор.
        result = f_readdir(&dir_psg_find, &fil_info);// != FR_OK){// && ((*ay->cfg->fil_info)->fname[0] != 0)){	// Читаем, пока файлы не кончатся (символ '0' означает, что файлы закончились). Главное, чтобы с картой все норм было.
        if (result != FR_OK) return OPEN_RED_DIR_ERROR;	// Если в процессе чтения произошла ошибка - выходим ни с чем.
        if (fil_info.fname[0] == 0){		// Если файлы на карте закончались - выходим.
            break;
        }
        // Если перед нами файл, смотрим по его разрешению, PSG это файл или нет.
        for (uint16_t l_byte = 0; l_byte < _MAX_LFN + 1; l_byte++){// Идем по всей длине файла. Если вдруг не указали расширения, то просто пропустим его.
            if (fil_info.fname[l_byte] == '.'){	   // Ищем, откуда идет разширение.
                if (_chack_psg_file(&fil_info.fname[l_byte]) != 1)	break;	// Если это не psg - к следущему файлу.

                // Теперь мы точно знаем, что перед нами файл с разрешением PSG. Проверяем его длину.
                int result_check_psg = _ay_psg_file_get_long(fd, fil_info.fname, ay->psg_file_buf);
                if (result_check_psg>0){	// Если длина определилась нормально.
                    // Переходим к нужной позиции в list файле.
                    if (f_lseek(&fil_psg_list, psg_file_loop*(256+4)) == FR_OK){		// Если все четко - дальеш.
                        if ((f_write(&fil_psg_list, fil_info.fname, 256, &rw_res) == FR_OK) && (rw_res == 256)){	// Если записалось имя как надо.
                            result_check_psg = result_check_psg*20/1000;	// Получаем колличество секунд.
                            if ((f_write(&fil_psg_list, &result_check_psg, 4, &rw_res) == FR_OK) && (rw_res == 4)){	// После имени еще 4 байта на размер.
                                psg_file_loop++;		// Мы нашли psg.
                                break;			// После этого выходим.
                            };
                        };
                    };
                };
                // Сюда отказо-устойчивый код, если что-то не так
            };
        };
    };

    result =  f_close(&fil_psg_list);						// Закрываем list файл.
    xSemaphoreGive(*ay->cfg->microsd_mutex);	// sdcard свободна.

    if (result<0) {	// Если в процессе чтения произошла ошибка - выдаем ее.
        return result;
    } else {
        ay->dir_number_file = psg_file_loop;	// Сохраняем, чтобы другие методы могли пользоваться.
        return psg_file_loop;
    };
    return 0;
}

// Получаем имя файла и его длительность по его номеру из отсортированного списка.
int ay_psg_file_get_name (int fd, uint32_t psg_file_number, char *buf_name, uint32_t *time){
    ay_file_mode_cfg_t *ay = eflib_getInstanceByFd (fd);
    FIL fil_get_name;				// Чтобы не мешать воспроизведению файла, создаем еще 1.
    //DIR dir_file;
    UINT l = 0;						// Ею будем отслеживать опустошение буффера. К тому же, в ней после считывания хранится число реально скопированныйх байт.
    xSemaphoreTake ( *ay->cfg->microsd_mutex, (TickType_t) portMAX_DELAY ); // Ждем, пока освободится microsd.

    volatile int res;
    // Если карта не открылась нормально - отдаем мутекс и выходим.
    res = f_open(&fil_get_name, "psg_list.list", FA_OPEN_EXISTING | FA_READ );
    if (res != FR_OK){
            xSemaphoreGive(*ay->cfg->microsd_mutex);
            return OPEN_FILE_ERROR;
    };

    if (f_lseek(&fil_get_name, psg_file_number*(256+4)) == FR_OK){	// Переходим к нужному файлу.
        if (f_read(&fil_get_name, buf_name, 256, &l) != FR_OK) {	// Читаем строку.
            return READ_FILE_ERROR;
        };
        if (f_read(&fil_get_name, time, 4, &l) != FR_OK){			// Читаем время.
            return READ_FILE_ERROR;
        };
    };

    f_close(&fil_get_name);			// Закрываем файл.

    xSemaphoreGive(*ay->cfg->microsd_mutex);
    return 0;
}

// Моя реализация интеллектуальной сортировки без учета регистра.
int _intelligent_sorting (char *string1, char *string2){
    char char_1, char_2;		// Буфферы для сравнения.
    int result = 0;				// -1 - 0-я меньше; 0 - равны; 1 - s2 больше.
    for (int loop_char = 0; loop_char < 256; loop_char++){ // Проходимся по всем символам строки.
        if ((uint8_t)string1[loop_char] == 0) { result = -1; return result;};
        if ((uint8_t)string2[loop_char] == 0) { result = 1; return result;};
        if (((uint8_t)string1[loop_char] >= (uint8_t)'A') && ((uint8_t)string1[loop_char] <= (uint8_t)'Z')){	// Если буква большая.
            char_1 = (char)((uint8_t)string1[loop_char] + (uint8_t)('a'-'A'));		// Делаем из большей малую.
        } else char_1 = string1[loop_char];
        if (((uint8_t)string2[loop_char] >= (uint8_t)'A') && ((uint8_t)string2[loop_char] <= (uint8_t)'Z')){	// Если буква большая.
            char_2 = (char)((uint8_t)string2[loop_char] + (uint8_t)('a'-'A'));		// Делаем из большей малую.
        } else char_2 = string2[loop_char];
        if ((uint8_t)char_1 < (uint8_t)char_2){
            result = -1;
            return result;
        } else if ((uint8_t)char_1 > (uint8_t)char_2){
            result = 1;
            return result;
        };
    };
    return result;
}

// Сортируем уже существующий список.
int ay_file_sort (int fd){
    ay_file_mode_cfg_t *ay = eflib_getInstanceByFd (fd);
    UINT wr_status;
    uint8_t flag; 	// Флаг указывает, какое из 2-х имен в буффере мы записали в microsd и его можно заменить новым.
    FIL fil_psg_list;				// Для записи в list_psg.
    if (f_open(&fil_psg_list, "psg_list.list", FA_OPEN_EXISTING | FA_READ | FA_WRITE) != FR_OK){
        return OPEN_FILE_ERROR;
    };

    for (uint32_t loop = 0; loop<ay->dir_number_file; loop++){ // Проходимся по n файлам n-1 раз.
        if (f_lseek(&fil_psg_list, 0) != FR_OK){return READ_FILE_ERROR;};
        while (f_read(&fil_psg_list, ay->psg_file_buf, 256 + 4, &wr_status) != FR_OK){};	// Копируем только имя + время.
        flag = 1;
        for (uint32_t file_loop = 1; file_loop < ay->dir_number_file; file_loop++){ //Считываем n-1 файлов в пары. После каждой итерации 1 файл записывается заменяя старый.
            if (f_lseek(&fil_psg_list, file_loop*(256+4)) != FR_OK){return READ_FILE_ERROR;};
            if (f_read(&fil_psg_list, &ay->psg_file_buf[flag*(256+4)], 256 + 4, &wr_status) != FR_OK){return READ_FILE_ERROR;};
            if (f_lseek(&fil_psg_list, (file_loop-1)*(256 + 4)) != FR_OK){return READ_FILE_ERROR;};
            int string_result =_intelligent_sorting((char*)ay->psg_file_buf, (char*)&ay->psg_file_buf[256+4]);	// Между временем и строкой 100% будет пробел. => время не тронут.
            if (string_result < 0){	// Если [0] строка меньше чем [256+4].
                if (f_write(&fil_psg_list, &ay->psg_file_buf[0], 256 + 4, &wr_status) != FR_OK){return READ_FILE_ERROR;};	// Обязательно записываем.
                flag = 0;
            } else {
                if (f_write(&fil_psg_list, &ay->psg_file_buf[256 + 4], 256 + 4, &wr_status) != FR_OK){return READ_FILE_ERROR;};
                flag = 1;
            };
        };
    };
    int res = f_close(&fil_psg_list);
    xSemaphoreGive(*ay->cfg->microsd_mutex);
    if (res != FR_OK){
        return OPEN_FILE_ERROR;
    }
    return 0;
}
*/

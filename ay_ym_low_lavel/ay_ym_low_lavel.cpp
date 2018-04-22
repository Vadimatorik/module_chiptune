#include "ay_ym_low_lavel.h"

AyYmLowLavel::AyYmLowLavel ( const ayYmLowLavelCfg* const cfg ) : cfg( cfg ) {
	this->s = USER_OS_STATIC_BIN_SEMAPHORE_CREATE( &this->sb );
}

void AyYmLowLavel::init ( void ) {
	USER_OS_STATIC_TASK_CREATE( this->task, "ayLow", AY_YM_LOW_LAVEL_TASK_STACK_SIZE, ( void* )this, this->cfg->taskPrio, this->tb, &this->ts );
}

// Выбираем нужные регистры в AY/YM.
/*
 * По сути, мы просто выдаем в расширитель портов то, что лежит в буффере по адресу p_sr_data.
 * Предполагается, что туда уже положили значения регистров для каждого AY/YM.
 * BDIR и BC1 дергаются так, чтобы произошел выбор регистра.
 */
void AyYmLowLavel::setReg ( void ) {
	if ( this->cfg->mutex != nullptr)
		USER_OS_TAKE_MUTEX( *this->cfg->mutex, portMAX_DELAY );

	this->cfg->sr->write();
	this->cfg->bc1->set();
	this->cfg->bdir->set();
	this->cfg->bdir->reset();
	this->cfg->bc1->reset();

	if ( this->cfg->mutex != nullptr)
		USER_OS_GIVE_MUTEX( *this->cfg->mutex );
}

//**********************************************************************
// Загружаем в заранее выбранный регистр значение.
// Предполагается, что в буффер по адресу p_sr_data
// уже были загружены нужные значения.
//**********************************************************************

void AyYmLowLavel::setData ( void ) {
	if ( this->cfg->mutex != nullptr)
		USER_OS_TAKE_MUTEX( *this->cfg->mutex, portMAX_DELAY );

	this->cfg->sr->write();
	this->cfg->bdir->set();
	this->cfg->bdir->reset();

	if ( this->cfg->mutex != nullptr)
		USER_OS_GIVE_MUTEX( *this->cfg->mutex );
}

void AyYmLowLavel::queueClear ( void ) {
	for ( int chip_loop = 0; chip_loop <  this->cfg->ayNumber; chip_loop++ ) {
		USER_OS_QUEUE_RESET( this->cfg->queueArray[ chip_loop ] );
	}
}

//**********************************************************************
// Вызывается в прерывании по переполнению таймера,
// настроенного на прерывание раз в 50 мс
// ( по умолчанию, значение может меняться ).
//**********************************************************************
void AyYmLowLavel::timerInterruptHandler ( void ) {
	this->cfg->timInterruptTask->clearInterruptFlag();
	static USER_OS_PRIO_TASK_WOKEN	 prio;
	USER_OS_GIVE_BIN_SEMAPHORE_FROM_ISR( this->s, &prio );	// Отдаем симафор и выходим (этим мы разблокируем поток, который выдает в чипы данные).
}

// true - если все очереди пусты.
bool AyYmLowLavel::queueEmptyCheck ( void ) {
	for (int chip_loop = 0; chip_loop <  this->cfg->ayNumber; chip_loop++) {
		if ( USER_OS_QUEUE_CHECK_WAIT_ITEM( this->cfg->queueArray[ chip_loop ] ) != 0 ) {
			return false;
		}
	}
	return true;
}

//**********************************************************************
// Запихиваем в очередь на выдачу в AY из массива данные.
// Данные должны быть расположены в формате регистр(16-8 бит)|значение(7-0 бит).
// Задача выполняется из под FreeRTOS.
//**********************************************************************
void AyYmLowLavel::queueAddElement ( ayQueueStruct* item ) {
	ayLowOutDataStruct buf;
	buf.reg	 = item->reg;
	buf.data	= item->data;

	USER_OS_QUEUE_SEND( this->cfg->queueArray[item->numberChip], &buf, portMAX_DELAY );
}

// Чистим все AY/YM без использования очереди. Предполагается, что при этом никак не может произойти выдача из очереди.
void AyYmLowLavel::hardwareClear ( void ) {
	//**********************************************************************
	// В каждом AY необходимо в регистр R7 положить 0b111111 (чтобы остановить генерацию звука и шумов).
	// А во все остальные регистры 0.
	//**********************************************************************
	for ( uint32_t loop_chip = 0; loop_chip < this->cfg->ayNumber; loop_chip++ ) {
		for ( uint32_t reg_l = 0; reg_l < 7; reg_l++ )
			this->buf_data_chip[ loop_chip ].reg[ reg_l ] = 0;
		this->buf_data_chip[ loop_chip ].reg[ 7 ] = 0b111111;
		for ( uint32_t reg_l = 8; reg_l < 16; reg_l++ )
			this->buf_data_chip[ loop_chip ].reg[ reg_l ] = 0;
	 }
	this->sendBuffer();
}

// Производим перестоновку бит в байте (с учетом реального подключения.
uint8_t AyYmLowLavel::connectionTransformation ( const uint8_t chip, const uint8_t& data ) {
	uint8_t buffer = 0;
	for ( uint8_t loop = 0; loop < 8; loop++ ) {
		buffer |= ( ( data & ( 1 << loop ) ) >> loop ) << this->cfg->connectCfg[ chip ].bias_bit[loop];
	}
	return buffer;
}

void AyYmLowLavel::resetFlagWait ( bool* flag_array ) {
	for ( uint32_t chip_loop = 0; chip_loop <  this->cfg->ayNumber; chip_loop++ ) {
		flag_array[ chip_loop ] = false;
	}
}

// False - когда все флаги False.
bool AyYmLowLavel::chackFlagWait ( bool* flag_array ) {
	for ( uint32_t chip_loop = 0; chip_loop <  this->cfg->ayNumber; chip_loop++ )
		if ( flag_array[ chip_loop ] == false ) return true;
	return false;
}

// Отправляем данные на все чипы из буфера.
// Это нужно либо для очистки (когда буфер пуст изначально), либо для восстановления после паузы.
void AyYmLowLavel::sendBuffer ( void ) {
	for ( uint32_t reg_loop = 0; reg_loop < 16; reg_loop++ ) {
		for ( uint32_t loop_ay = 0; loop_ay < this->cfg->ayNumber; loop_ay++ )
			this->cfg->srData[ loop_ay ] = this->connectionTransformation( loop_ay, reg_loop );
		this->setReg();

		for ( uint32_t loop_ay = 0; loop_ay < this->cfg->ayNumber; loop_ay++ )
			this->cfg->srData[ loop_ay ] = this->connectionTransformation( loop_ay, this->buf_data_chip[ loop_ay ].reg[ reg_loop ] );
		this->setData();
	}
}

//**********************************************************************
// Данный поток будет выдавать из очереди 50 раз
// в секунду данные (данные разделяются 0xFF).
// Разблокируется из ay_timer_handler.
//**********************************************************************

void AyYmLowLavel::task ( void* p_this ) {
	AyYmLowLavel*			obj = ( AyYmLowLavel* ) p_this;
	ayLowOutDataStruct	  buffer[ obj->cfg->ayNumber ];		  // Буфер для адрес/команда для всех чипов.
	bool						flag_wait[ obj->cfg->ayNumber ];	   // True - в этом прерывании уже не обрабатываем эту очередь.
	ayChipReg buf_data_chip[ obj->cfg->ayNumber ];
	obj->buf_data_chip		  = buf_data_chip;

	while( true ) {
		obj->resetFlagWait( flag_wait );							  // Новое прерывание, все флаги можно сбросить.
		USER_OS_TAKE_BIN_SEMAPHORE ( obj->s, portMAX_DELAY );   // Как только произошло прерывание (была разблокировка из ay_timer_handler).
		if ( obj->queueEmptyCheck() == true ) continue;			   // Если в очередях пусто - выходим.

		while ( obj->chackFlagWait( flag_wait ) ) {
			// Собираем из всех очередей пакет регистр/значение.
			for ( uint32_t chip_loop = 0; chip_loop <  obj->cfg->ayNumber; chip_loop++ ) {
				if ( flag_wait[chip_loop] ) continue;
				USER_OS_QUEUE_CHECK_WAIT_ITEM( obj->cfg->queueArray[ chip_loop ] );
				uint32_t count = uxQueueMessagesWaiting( obj->cfg->queueArray[chip_loop] );
				if ( count != 0 ) {																		 // Если для этого чипа очередь не пуста.
					USER_OS_QUEUE_RECEIVE( obj->cfg->queueArray[ chip_loop ], &buffer[ chip_loop ], 0 );   // Достаем этот элемент без ожидания, т.к. точно знаем, что он есть.
					if ( buffer[ chip_loop ].reg == 0xFF ) {	// Если это флаг того, что далее читать можно лишь в следущем прерывании,...
						buffer[chip_loop].reg = 17;			 // Если этот чип уже неактивен, то пишем во внешний регистр (пустоту). Сейчас и далее.
						flag_wait[chip_loop] = true;			// Защищаем эту очередь от последущего считывания в этом прерывании.
					} else {	// Дублируем в памяти данные чипов.
						obj->buf_data_chip[ chip_loop ].reg[ buffer[ chip_loop ].reg ] =  buffer[chip_loop].data;
					}
				} else {
					buffer[chip_loop].reg = 17;
					flag_wait[chip_loop] = true;	 // Показываем, что в этой очереди закончились элементы.
				}
			}

			//**********************************************************************
			// Собранный пакет раскладываем на регистры и на их значения и отправляем.
			//**********************************************************************
			for ( uint32_t chip_loop = 0; chip_loop <  obj->cfg->ayNumber; chip_loop++ )
				obj->cfg->srData[chip_loop] = obj->connectionTransformation( chip_loop, buffer[chip_loop].reg );

			obj->setReg();
			for ( uint32_t chip_loop = 0; chip_loop <  obj->cfg->ayNumber; chip_loop++ )
				obj->cfg->srData[chip_loop] = obj->connectionTransformation( chip_loop, buffer[chip_loop].data );

			obj->setData();
		}
		//**********************************************************************
		// В случае, если идет отслеживание секунд воспроизведения, то каждую секунду отдаем симафор.
		//**********************************************************************
		obj->tic_ff++;
		if ( obj->tic_ff != 50 )						continue;
			obj->tic_ff = 0;// Если насчитали секунду.
		if ( obj->cfg->semaphoreSecOut == nullptr )   continue;		 // Если есть соединение семофором, то отдать его.
			USER_OS_GIVE_BIN_SEMAPHORE( *obj->cfg->semaphoreSecOut );
	}
}

// Останавливаем/продолжаем с того же места воспроизведение. Синхронно для всех AY/YM.
void AyYmLowLavel::playStateSet ( uint8_t state ) {
	this->cfg->pwrSet( state );
	if ( state == 1 ) {
		this->cfg->timFrequencyAy->on();
		this->cfg->timInterruptTask->on();
		this->sendBuffer();											// Восстанавливаем контекст.
	} else {
		this->cfg->timInterruptTask->off();
		this->cfg->timFrequencyAy->off();
	};
}

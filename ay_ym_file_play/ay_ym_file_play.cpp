#include "ay_ym_file_play.h"

#ifdef MODULE_AY_YM_FILE_PLAY_ENABLED

#define CHACK_CALL_FUNC_ANSWER( r )	if ( r != 0 )	return r;

/// Считываемый из файла пакет.
struct __attribute__((packed)) packetPsg {
	uint8_t		reg;
	uint8_t		data;
};

int AyYmFilePlayBase::psgFilePlay ( void ) {
	int	r;

	/// Открываем файл, который планируем воспроизводить.
	r	=	this->openFile();
	CHACK_CALL_FUNC_ANSWER( r );

	/// Включаем чип.
	r	=	this->setPwrChip( true );
	CHACK_CALL_FUNC_ANSWER( r );

	/// Производим начальную инициализацию чипа.
	r	=	this->initChip();
	CHACK_CALL_FUNC_ANSWER( r );

	/// В данных переменных будет храниться
	/// данные "регистр/данные".
	packetPsg			packet;

	/// Получаем длину файла.
	uint32_t	fileSize;
	r	=	this->getFileLen( fileSize );
	CHACK_CALL_FUNC_ANSWER( r );

	/*!
	 * Далее начинается анализ файла.
	 */

	/// Количество оставшихся байт в файле.
	uint32_t	countRemainingBytes;

	/// Данные начинаются с 4-го или 16-го байта.
	r	=	this->setOffsetByteInFile( 3 );			/// Проверим маркер (3-й байт).
	CHACK_CALL_FUNC_ANSWER( r );

	/// Првоеряем, полный заголовок или нет.
	uint8_t		buf;
	r	=	this->readInArray( &buf, 1 );

	/// Если этот флаг, то заголовок полный
	/// и данные начинаются с 16-го байта.
	if ( buf == 0x1A ) {
		/// Первые 16 байт - заголовок. Пропускаем.
		countRemainingBytes = fileSize - 16;

		r	=	this->setOffsetByteInFile( 16 );
		CHACK_CALL_FUNC_ANSWER( r );
	} else {
		/// Первые 3 байта заголвок.
		countRemainingBytes = fileSize - 3;
	}

	/*!
	 * Что ожидается увидеть.
	 * false		-	мы ждем регистр или маркер.
	 * true			-	данные.
	 */
	bool	expectedAppointment	=	false;

	/// Анализируем весь файл.
	while( countRemainingBytes ) {
		/// Считываем маркер.
		r	=	this->readInArray( &buf, 1 );
		CHACK_CALL_FUNC_ANSWER( r );
		countRemainingBytes--;

		/*!
		 * Предыдущий байт был регистром - этот 100% данные.
		 */
		if ( expectedAppointment == true ) {
			expectedAppointment = false;

			/// У нас считан регистр AY или левого устройства.
			if ( packet.reg < 16 ) {											/// Регистр для AY.
				packet.data	=	buf;
				r	=	this->writePacket( packet.reg, packet.data );
				CHACK_CALL_FUNC_ANSWER( r );
			};
			continue;
		}

		/// Далее разбираем маркеры или регистр.

		/*!
		 * Если файл не поврежден, тогда маркером
		 * может быть только 0xFF или 0xFE.
		 */
		if ( buf == 0xFF ) {								/// 0xFF.
			/// 0xFF признак того, что произошло прерывание.
			r	=	this->sleepChip( 1 );
			CHACK_CALL_FUNC_ANSWER( r );
			continue;
		};

		if ( buf == 0xFE ) {								/// 0xFE.
			/// За 0xFE следует байт, который при *4 даст колличество
			/// прерываний, во время которых на AY не приходит никаких данных.
			r	=	this->readInArray( &buf, 1 );
			CHACK_CALL_FUNC_ANSWER( r );
			countRemainingBytes--;

			buf *= 4;
			r	=	this->sleepChip( buf );
			CHACK_CALL_FUNC_ANSWER( r );

			continue;
		}

		if ( buf == 0xFD ) {										/// Флаг конца трека.
			break;
		}

		/// Иначе это регистр.
		packet.reg				=	buf;
		expectedAppointment		=	true;							/// Ждем данные.
	}

	/// Отключаем чип.
	r	=	this->setPwrChip( false );
	CHACK_CALL_FUNC_ANSWER( r );

	/// Закрываем файл.
	r	=	this->closeFile();
	CHACK_CALL_FUNC_ANSWER( r );

	return 0;
}

int AyYmFilePlayBase::psgFileGetLong ( uint32_t& resultLong ) {
	int	r;

	resultLong = 0;

	/// Открываем файл, который планируем воспроизводить.
	r	=	this->openFile();
	CHACK_CALL_FUNC_ANSWER( r );

	/// Получаем длину файла.
	uint32_t	fileSize;
	r	=	this->getFileLen( fileSize );
	CHACK_CALL_FUNC_ANSWER( r );

	/*!
	 * Далее начинается анализ файла.
	 */

	/// Количество оставшихся байт в файле.
	uint32_t	countRemainingBytes;

	/// Данные начинаются с 4-го или 16-го байта.
	r	=	this->setOffsetByteInFile( 3 );			/// Проверим маркер (3-й байт).
	CHACK_CALL_FUNC_ANSWER( r );

	/// Првоеряем, полный заголовок или нет.
	uint8_t		buf;
	r	=	this->readInArray( &buf, 1 );

	/// Если этот флаг, то заголовок полный
	/// и данные начинаются с 16-го байта.
	if ( buf == 0x1A ) {
		/// Первые 16 байт - заголовок. Пропускаем.
		countRemainingBytes = fileSize - 16;

		r	=	this->setOffsetByteInFile( 16 );
		CHACK_CALL_FUNC_ANSWER( r );
	} else {
		/// Первые 3 байта заголвок.
		countRemainingBytes = fileSize - 3;
	}

	/*!
	 * Что ожидается увидеть.
	 * false		-	мы ждем регистр или маркер.
	 * true			-	данные.
	 */
	bool	expectedAppointment	=	false;

	/// Анализируем весь файл.
	while( countRemainingBytes ) {
		/// Считываем маркер.
		r	=	this->readInArray( &buf, 1 );
		CHACK_CALL_FUNC_ANSWER( r );
		countRemainingBytes--;

		/*!
		 * Предыдущий байт был регистром - этот 100% данные.
		 */
		if ( expectedAppointment == true ) {
			expectedAppointment = false;
			continue;
		}

		/// Далее разбираем маркеры или регистр.

		/*!
		 * Если файл не поврежден, тогда маркером
		 * может быть только 0xFF или 0xFE.
		 */
		if ( buf == 0xFF ) {								/// 0xFF.
			resultLong++;
			continue;
		};

		if ( buf == 0xFE ) {								/// 0xFE.
			r	=	this->readInArray( &buf, 1 );
			CHACK_CALL_FUNC_ANSWER( r );
			countRemainingBytes--;

			buf *= 4;
			resultLong += buf;

			continue;
		}

		if ( buf == 0xFD ) {										/// Флаг конца трека.
			break;
		}

		/// Иначе это регистр.
		expectedAppointment		=	true;							/// Ждем данные.
	}

	/// Не забываем закрыть файл.
	r	=	closeFile();
	return r;
}

#endif


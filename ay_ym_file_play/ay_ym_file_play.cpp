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

	/// Данные начинаются с 16-го байта.
	r	=	this->setOffsetByteInFile( 16 );
	CHACK_CALL_FUNC_ANSWER( r );

	/*!
	 * Далее начинается анализ файла.
	 */

	/// Количество оставшихся байт в файле.
	uint32_t	countRemainingBytes;

	/// Первые 16 байт - заголовок. Пропускаем.
	countRemainingBytes = fileSize - 16;

	/// Анализируем весь файл.
	while( countRemainingBytes ) {
		/// Считываем маркер.
		uint8_t		buf;
		r	=	this->readInArray( &buf, 1 );
		CHACK_CALL_FUNC_ANSWER( r );
		countRemainingBytes--;

		/*!
		 * Если файл не поврежден, тогда маркером
		 * может быть только 0xFF или 0xFE.
		 */
		if ( buf == 0xFF ) {								/// 0xFF.
			/// 0xFF признак того, что произошло прерывание.
			r	=	this->sleepChip( 1 );
			CHACK_CALL_FUNC_ANSWER( r );

			/// Считываем пакет "регистр + значение".
			r	=	this->readInArray( ( uint8_t* )&packet, 2 );
			CHACK_CALL_FUNC_ANSWER( r );
			countRemainingBytes	-= 2;

			/// Отправляем в чип пару "регистр + значение".
			r	=	this->writePacket( packet.reg, packet.data );
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

		/// У нас считан регистр AY или левого устройства.
		if ( buf < 16 ) {											/// Регистр для AY.
			packet.reg		=	buf;								/// Ранее был считан регистр.

			/// Считываем данные регистра.
			r	=	this->readInArray( &packet.data, 1 );
			CHACK_CALL_FUNC_ANSWER( r );
			countRemainingBytes--;

			/// Отправляем в чип пару "регистр + значение".
			r	=	this->writePacket( packet.reg, packet.data );
			CHACK_CALL_FUNC_ANSWER( r );
		} else {													/// Пишут не в AY.
			r	=	this->readInArray( &buf, 1 );			/// Считываем байт в пустоту.
			CHACK_CALL_FUNC_ANSWER( r );
			countRemainingBytes--;
		}
	}

	/// Отключаем чип.
	r	=	this->setPwrChip( false );
	CHACK_CALL_FUNC_ANSWER( r );

	/// Открываем файл, который планируем воспроизводить.
	r	=	this->closeFile();
	CHACK_CALL_FUNC_ANSWER( r );

	return 0;
}

int AyYmFilePlayBase::psgFileGetLong ( uint32_t& resultLong ) {
	int	r;

	/// Открываем файл, который планируем просканировать.
	r	=	this->openFile();
	CHACK_CALL_FUNC_ANSWER( r );

	/// В данных переменных будет храниться
	/// данные "регистр/данные".
	packetPsg			packet;

	/// Получаем длину файла.
	uint32_t	fileSize;
	r	=	this->getFileLen( fileSize );
	CHACK_CALL_FUNC_ANSWER( r );

	/// Данные начинаются с 16-го байта.
	r	=	this->setOffsetByteInFile( 16 );
	CHACK_CALL_FUNC_ANSWER( r );

	/*!
	 * Далее начинается анализ файла.
	 */

	resultLong = 0;

	/// Количество оставшихся байт в файле.
	uint32_t	countRemainingBytes;

	/// Первые 16 байт - заголовок. Пропускаем.
	countRemainingBytes = fileSize - 16;

	/// Анализируем весь файл.
	while( countRemainingBytes ) {
		/// Считываем маркер.
		uint8_t		buf;
		r	=	this->readInArray( &buf, 1 );
		CHACK_CALL_FUNC_ANSWER( r );
		countRemainingBytes--;

		/*!
		 * Если файл не поврежден, тогда маркером
		 * может быть только 0xFF или 0xFE.
		 */
		if ( buf == 0xFF ) {								/// 0xFF.
			resultLong++;

			/// Считываем пакет "регистр + значение".
			r	=	this->readInArray( ( uint8_t* )&packet, 2 );
			CHACK_CALL_FUNC_ANSWER( r );
			countRemainingBytes	-= 2;

			continue;
		};

		if ( buf == 0xFE ) {								/// 0xFE.
			/// За 0xFE следует байт, который при *4 даст колличество
			/// прерываний, во время которых на AY не приходит никаких данных.
			r	=	this->readInArray( &buf, 1 );
			CHACK_CALL_FUNC_ANSWER( r );
			countRemainingBytes--;

			buf *= 4;
			resultLong += buf;

			continue;
		}

		/// У нас считан регистр AY или левого устройства.
		r	=	this->readInArray( &buf, 1 );			/// Считываем байт в пустоту.
		CHACK_CALL_FUNC_ANSWER( r );
		countRemainingBytes--;
	}

	/// Открываем файл, который планируем воспроизводить.
	r	=	this->closeFile();
	CHACK_CALL_FUNC_ANSWER( r );

	return 0;
}

#endif


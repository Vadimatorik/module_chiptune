#pragma once

#include "project_config.h"

#ifdef MODULE_AY_YM_FILE_PLAY_ENABLED

#include <stdint.h>
#include <errno.h>
#include <memory>

/*!
 * Цель прохода по файлу.
 */
enum class PARSE_TYPE {
	PLAY			=	0,				/// Воспроизведение трека.
	GET_LONG		=	1				/// Подсчет длительности.
};

class AyYmFilePlayBase {
public:
	/*!
	 * Преобразует psg файл в структуры "регистр/значение".
	 *
	 * \return			{	0			-	успех выполнения операции.
	 * 						-1			-	трек был остановлен.
	 * 						ENOEXEC		-	размер файла меньше 16 байт.	}
	 */
	int		psgFilePlay					(	std::shared_ptr< char >		fullFilePath	);

	/*!
	 * Получает длину psg файла в "тиках" (количестве прерываний).
	 *
	 * \param[out]		resultLong		-	длительность файла в "тиках"
	 * 											(прерываниях AY).
	 * \return			{	0			-	успех выполнения операции.
	 * 						>0			-	провал операции.
	 * 						ENOEXEC		-	размер файла меньше 16 байт.	}
	 */
	int		psgFileGetLong				(	std::shared_ptr< char >		fullFilePath,
											uint32_t&					resultLong	);

private:
	/*!
	 * Поскольку воспроизведение и получение длины имеют
	 * одинаковый алгоритм анализа входного файла, с той
	 * лишь разницей, что в одном случае идет подсчет
	 * происходящих прерываний, а в другой отправка данных
	 * регистров и факта прерывания в прослойку, реализующую
	 * воспроизведение на чипе, то разумно объединить
	 * методы подсчета длины и воспроизведения в один метод.
	 *
	 * В случае, если type == PLAY, то параметр
	 * resultLong игнорируется и может быть nullptr.
	 */
	int		psgFileParse				(	PARSE_TYPE			type,
											uint32_t*			resultLong	);

	/*!
	 * При вызове методов, обращающихся к файлу
	 * воспроизводимого трека предполагается, что
	 * все проблемы, связанные с файлом будут
	 * решены в пределах этих методов.
	 *
	 * В случае, если же решить проблему не удастся,
	 * требуется возвратить соответствующий код ошибки.
	 * После этого преобразование будет аварийно прекращено.
	 *
	 * Флаг ошибки будет передан на более высокий уровень:Я
	 * метод, который вызвал метод воспроизведения трека.
	 */

	/*!
	 * Открывает файл.
	 *
	 * \return			{	0	-	успех выполнения операции.
	 * 						>0	-	провал операции.	}
	 */
	virtual int	openFile						(	std::shared_ptr< char >		fullFilePath	)		= 0;

	/*!
	 * Закрывает файл.
	 *
	 * \return			{	0	-	успех выполнения операции.
	 * 						>0	-	провал операции.	}
	 */
	virtual int	closeFile						(	void	)								= 0;

	/*!
	 * Возвращает длину файла в байтах.
	 *
	 * \param[out]		returnFileLenByte		-	длина воспроизводимого трека в байтах.
	 *
	 * \return			{	0	-	успех выполнения операции.
	 * 						-1	-	трек был остановлен.
	 * 						>0	-	провал операции.	}
	 */
	virtual int	getFileLen						(	uint32_t&		returnFileLenByte	)	= 0;

	/*!
	 * Устанавливает смещение от начала в файле.
	 * \param[in]		offsetByteInFile		-	смещение от начала файла в байтах.
	 * 												Может быть 0.
	 *
	 * \return			{	0	-	успех выполнения операции.
	 * 						-1	-	трек был остановлен.
	 * 						>0	-	провал операции.	}
	 */
	virtual int	setOffsetByteInFile				(	const uint32_t	offsetByte	)			= 0;

	/*!
	 * Считывает в буфер требуемое количество байт.
	 *
	 * \param[out]		returnDataBuffer		-	буфер, в который будут считанные данные.
	 * \param[in]		countByteRead			-	количество считываемых байт.
	 *
	 * \return			{	0	-	успех выполнения операции.
	 * 						-1	-	трек был остановлен.
	 * 						>0	-	провал операции.	}
	 */
	virtual int	readInArray					(	uint8_t*		returnDataBuffer,
												const uint32_t	countByteRead	)			= 0;

	/*!
	 * Включает/отключает чип (подает/снимает питание).
	 *
	 * \param[in]		state					-	true: включить чип; false: отключить.
	 *
	 * \return			{	0	-	успех выполнения операции.
	 * 						>0	-	провал операции.	}
	 */
	virtual int	setPwrChip					(	bool	state	)							= 0;

	/*!
	 * Инициализирует чип.
	 *
	 * \return			{	0	-	успех выполнения операции.
	 * 						>0	-	провал операции.	}
	 */
	virtual int	initChip					(	void	)									= 0;

	/*!
	 * Сообщает чипу, что следующие countTick прерываний
	 * не будет обновлений данных регистров.
	 *
	 * \param[in]		countTick				-	количество прерываний,
	 * 												в течении которых на AY не будет
	 * 												изменения параметров.
	 * 												>0.
	 *
	 * \return			{	0	-	успех выполнения операции.
	 * 						-1	-	трек был остановлен.
	 * 						>0	-	провал операции.	}
	 */
	virtual int	sleepChip					(	const uint32_t	countTick	)				= 0;

	/*!
	 * Отправляет в AY пару "регистр + значение".
	 *
	 * \param[in]		reg						-	регистр AY, в который будут
	 * 												записаны данные.
	 * \param[in]		data					-	данные, которые будут записаны в регистр.
	 *
	 * \return			{	0	-	успех выполнения операции.
	 * 						-1	-	трек был остановлен.
	 * 						>0	-	провал операции.	}
	 */
	virtual int	writePacket					(	const uint8_t	reg,
												const uint8_t	data	)					= 0;

private:
	std::shared_ptr< char >			pathToFile;

};

#endif






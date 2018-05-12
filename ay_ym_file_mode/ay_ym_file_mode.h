#pragma once

#ifdef MODULE_AY_YM_FILE_MODE_ENABLED

#include "ay_ym_low_lavel.h"			// Для обращения к используемому AY.
#include "ff.h"							// FatFS от ChaN.
#include "user_os.h"					// Пользовательская OS.

// Структура первоначальной инициализации AY чипа.
struct ayYmFileModeCfg {
	AyYmLowLavel*				ay;
	void ( *pwrChipOn )		( uint32_t chip, bool state );		// chip - номер чипа, который следует включить/выключить. state == true - включить.
	// ВАЖНО! ay_low может полностью обесточивать всю аналоговую часть. Тут же управление конкретными чипами!
};

enum class EC_AY_FILE_MODE_ANSWER {
	OK						= 0,
	ARG_ERROR,											 // Ошибка входного аргумента ( например, попросили сыграть 2-й файл, когда в папке всего 1 ).
	WRITE_FILE_ERROR,
	OPEN_FILE_ERROR,
	OPEN_DIR_ERROR,
	OPEN_READ_DIR_ERROR,
	READ_FILE_ERROR,
	FIND_ERROR,
	TRACK_END,												// Трек закончился сам успешно.
	TRACK_STOPPED,											// Трек остановили насильно.
};

class AyYmFileMode {
public:
	AyYmFileMode ( ayYmFileModeCfg* cfg );

	//**********************************************************************
	// Воспроизводим psg файл.
	// Важно! В папке по пути dir_path лолжен существовать файл списка воспроизведения.
	//**********************************************************************
	EC_AY_FILE_MODE_ANSWER	psgFilePlay					( char* full_name_file, uint8_t number_chip );

	// Завершает psg_file_play из другого потока.
	void					psgFileStop					( void );											// Останавливакем воспроизведение.
																	// валидных psg файлов.
	// Получаем длину файла (если валидный).
	// Файл должен находится в текущей директории.
	EC_AY_FILE_MODE_ANSWER	psgFileGetLong				( char* name,
														  uint32_t& resultLong );

private:
	EC_AY_FILE_MODE_ANSWER	psgPartCopyFromSdToArray	( uint32_t sektor,
														  uint16_t pointBuffer,
														  uint8_t numberSector,
														  UINT *l );


	// Ждем, пока все данные из очереди ay низкого уровня будут переданы (файл будет воиспроизведен до конца).
	void					ayDelayLowQueueClean		( void );

	// Очищаем чип через очередь.
	void					clearChip					( uint8_t chipNumber );

	const ayYmFileModeCfg* const cfg;

	bool		emergency_team = false;
};

#endif






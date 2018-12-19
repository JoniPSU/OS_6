// ThreadWar.cpp : Defines the entry point for the console application.

//
//      Простая компьютерная игра Thread War

//      Thread War'

//      Используйте клавиши "влево" и "вправо", чтобы перемещать пушку

//      клавиша "пробел" производит выстрел

//      Если 30 врагов уйдут с экрана неуничтоженными,  вы проиграли

//      Очки даются за каждого убитого противника

#include <windows.h>

#include <process.h>

#include <stdlib.h>

#include <time.h>

#include <stdio.h>

// Объекты синхронизации

HANDLE screenlock; // изменением экрана занимается только один поток
HANDLE bulletsem;  // можно выстрелить только три раза подряд
HANDLE startevt;     // игра начинается с нажатием клавиши "влево" или "вправо"
HANDLE conin, conout;  // дескрипторы консоли
HANDLE mainthread; // Основной поток main
CRITICAL_SECTION gameover;
CONSOLE_SCREEN_BUFFER_INFO info; // информация о консоли
								 // количество попаданий и промахов
long hit = 0;
long miss = 0;
long delayfactor = 7; // фактор задержки для

					  // Создание случайного числа от n0 до n1

int random(int n0, int n1)

{

	if (n0 == 0 && n1 == 1) return rand() % 2;

	return rand() % (n1 - n0) + n0;

}



// Очистка экрана консоли

void cls()
{
	COORD org = { 0,0 };
	DWORD res;
	FillConsoleOutputCharacter(conout, ' ', info.dwSize.X*info.dwSize.Y, org, &res);
}



// вывести на экран символ в позицию х и у

void writeat(int x, int y, char c)
{
	// Блокировать вывод на экран при помощи мьютекса
	WaitForSingleObject(screenlock, INFINITE);
	COORD pos = { x,y };
	DWORD res;
	WriteConsoleOutputCharacter(conout, &c, 1, pos, &res);
	ReleaseMutex(screenlock);
}

// Получить нажатие на клавишу (счетчик повторейний в ct)

int getakey(int &ct)
{

	INPUT_RECORD input;
	DWORD res;
	while (1)
	{
		ReadConsoleInput(conin, &input, 1, &res);
		// игнорировать другие события
		if (input.EventType != KEY_EVENT) continue;
		// игнорировать события отпускания клавиш
		// нас интересуют только нажатия
		if (!input.Event.KeyEvent.bKeyDown) continue;
		ct = input.Event.KeyEvent.wRepeatCount;
		return input.Event.KeyEvent.wVirtualKeyCode;
	}
}

// Обработка комбинаций ^С. ^Break, и т.п.

BOOL WINAPI ctrl(DWORD type)
{
	exit(0);
	return TRUE; // не достижимый участок кода
}

// Определить символ в заданной позиции экрана

int getat(int x, int y)
{
	char c;
	DWORD res;
	COORD org = { x,y };
	// Блокировать доступ к консоли до тех пор, пока процедура не будет выполнена
	WaitForSingleObject(screenlock, INFINITE);
	ReadConsoleOutputCharacter(conout, &c, 1, org, &res);
	ReleaseMutex(screenlock); // unlock
	return c;
}

// Отобразить очки в заголовке окна и проверить условие завершения игры

void score(void)

{
	char s[128];
	sprintf(s, "Thread War! Hits:%d  Misses:%d", hit, miss);
	SetConsoleTitle(s);
	if (miss >= 30)
	{
		EnterCriticalSection(&gameover);
		SuspendThread(mainthread); // stop main thread
		MessageBox(NULL, "Game Over!", "Thread War", MB_OK | MB_SETFOREGROUND);
		exit(0); // не выходит из критической секции
	}
	if ((hit + miss) % 20 == 0)
		InterlockedDecrement(&delayfactor); // должен быть ilock

}

char badchar[] = "-\\|/";

// это поток противника

void badguy(void *_y)
{
	int y = (int)_y; // случайная координата у
	int dir;
	int x;
	// нечетные у появляются слева, четные у появляются справа
	x = y % 2 ? 0 : info.dwSize.X;
	// установить направление в зависимости от начальной позиции
	dir = x ? -1 : 1;
	// пока противник находится в пределах экрана
	while ((dir == 1 && x != info.dwSize.X) || (dir == -1 && x != 0))
	{
		int dly;
		BOOL hitme = FALSE;
		// проверка на попадание (пуля?)
		if (getat(x, y) == '*') hitme = TRUE;
		// вывод символа на экран
		writeat(x, y, badchar[x % 4]);
		// еще одна проверка на попадание
		if (getat(x, y) == '*') hitme = TRUE;
		// проверка на попадание через небольшие
		//  промежутки времени
		if (delayfactor<3) dly = 3;
		else dly = delayfactor +3;
		for (int i = 0;i<dly;i++)
		{
			Sleep(40);
			if (getat(x, y) == '*')
			{
				hitme = TRUE;
				break;
			}
		}
		writeat(x, y, ' ');
		// еще одна проверка на попадание
		if (getat(x, y) == '*') hitme = TRUE;
		if (hitme)
		{
			// в противника попали!
			MessageBeep(-1);
			InterlockedIncrement(&hit);
			score();
			_endthread();
		}
		x += dir;
	}
	// противник убежал!
	InterlockedIncrement(&miss);
	score();
}



// этот поток занимается созданием потоков противников

void badguys(void *)

{
	// ждем сигнала к началу игры в течение 15 секунд
	WaitForSingleObject(startevt, 15000);
	// создаем случайного врага
	// каждые 5 секунд появляется шанс создать
	// противника с координатами от 1 до 10
	while (1)
	{
		if (random(0, 100)<(hit + miss) / 25 + 20)
			// со временем вероятность увеличивается
			_beginthread(badguy, 0, (void *)(random(1, 10)));
		Sleep(1000); // каждую секунду
	}
}


// Это поток пули
// каждая пуля - это отдельный поток
void bullet(void *_xy_)
{
	COORD xy = *(COORD *)_xy_;
	if (getat(xy.X, xy.Y) == '*') return; // здесь уже есть пуля
										  // надо подождать
										  // Проверить семафор
										  // если семафор равен 0, выстрела не происходит
	if (WaitForSingleObject(bulletsem, 0) == WAIT_TIMEOUT) return;
	while (--xy.Y)
	{
		writeat(xy.X, xy.Y, '*'); // отобразить пулю
		Sleep(100);
		writeat(xy.X, xy.Y, ' ');    // стереть пулю
	}
	// выстрел сделан.- добавить 1 к семафору
	ReleaseSemaphore(bulletsem, 1, NULL);
}
// Основная программа

void main()

{
	HANDLE me;
	// Настройка глобальных переменных
	conin = GetStdHandle(STD_INPUT_HANDLE);
	conout = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleCtrlHandler(ctrl, TRUE);
	SetConsoleMode(conin, ENABLE_WINDOW_INPUT);
	me = GetCurrentThread(); // не является реальным дескриптором
							 // изменить псевдодескриптор на реальный дескриптор текущего потока
	DuplicateHandle(GetCurrentProcess(), me, GetCurrentProcess(), &mainthread, 0, FALSE, DUPLICATE_SAME_ACCESS);
	startevt = CreateEvent(NULL, TRUE, FALSE, NULL);
	screenlock = CreateMutex(NULL, FALSE, NULL);
	InitializeCriticalSection(&gameover);
	bulletsem = CreateSemaphore(NULL, 3,3, NULL);
	GetConsoleScreenBufferInfo(conout, &info);
	// Инициализировать отображение информации об очках
	score();
	// Настроить генератор случайных чисел
	srand((unsigned)time(NULL));
	cls(); // на самом деле не нужно
		   // установка начальной позиции пушки
	int y = info.dwSize.Y - 1;
	int x = info.dwSize.X / 2;
	// запустить поток badguys; ничего не делать до тех пор,
	// пока не произойдет событие или истекут 15 секунд
	_beginthread(badguys, 0, NULL); // основной цикл игры
	while (1)
	{
		int c, ct;
		writeat(x, y, '|'); // нарисовать пушку
		c = getakey(ct);   // получить символ
		switch (c)
		{
		case VK_SPACE:
		{
			static COORD xy;
			xy.X = x;
			xy.Y = y;
			_beginthread(bullet, 0, (void *)&xy);
			Sleep(100); // дать пуле время улететь на некоторое расстояние
			break;
		}

		case VK_LEFT:  // команда "влево!"

			SetEvent(startevt);    // поток badguys работает

			writeat(x, y, ' ');      // убрать с экрана пушку

			while (ct--)        // переместиться

				if (x) x--;

			break;

		case VK_RIGHT: // команда "вправо!"; логика та же

			SetEvent(startevt);

			writeat(x, y, ' ');

			while (ct--)

				if (x != info.dwSize.X - 1) x++;

			break;

		}
	}
}
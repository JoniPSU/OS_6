// ThreadWar.cpp : Defines the entry point for the console application.

//
//      ������� ������������ ���� Thread War

//      Thread War'

//      ����������� ������� "�����" � "������", ����� ���������� �����

//      ������� "������" ���������� �������

//      ���� 30 ������ ����� � ������ ���������������,  �� ���������

//      ���� ������ �� ������� ������� ����������

#include <windows.h>

#include <process.h>

#include <stdlib.h>

#include <time.h>

#include <stdio.h>

// ������� �������������

HANDLE screenlock; // ���������� ������ ���������� ������ ���� �����
HANDLE bulletsem;  // ����� ���������� ������ ��� ���� ������
HANDLE startevt;     // ���� ���������� � �������� ������� "�����" ��� "������"
HANDLE conin, conout;  // ����������� �������
HANDLE mainthread; // �������� ����� main
CRITICAL_SECTION gameover;
CONSOLE_SCREEN_BUFFER_INFO info; // ���������� � �������
								 // ���������� ��������� � ��������
long hit = 0;
long miss = 0;
long delayfactor = 7; // ������ �������� ���

					  // �������� ���������� ����� �� n0 �� n1

int random(int n0, int n1)

{

	if (n0 == 0 && n1 == 1) return rand() % 2;

	return rand() % (n1 - n0) + n0;

}



// ������� ������ �������

void cls()
{
	COORD org = { 0,0 };
	DWORD res;
	FillConsoleOutputCharacter(conout, ' ', info.dwSize.X*info.dwSize.Y, org, &res);
}



// ������� �� ����� ������ � ������� � � �

void writeat(int x, int y, char c)
{
	// ����������� ����� �� ����� ��� ������ ��������
	WaitForSingleObject(screenlock, INFINITE);
	COORD pos = { x,y };
	DWORD res;
	WriteConsoleOutputCharacter(conout, &c, 1, pos, &res);
	ReleaseMutex(screenlock);
}

// �������� ������� �� ������� (������� ����������� � ct)

int getakey(int &ct)
{

	INPUT_RECORD input;
	DWORD res;
	while (1)
	{
		ReadConsoleInput(conin, &input, 1, &res);
		// ������������ ������ �������
		if (input.EventType != KEY_EVENT) continue;
		// ������������ ������� ���������� ������
		// ��� ���������� ������ �������
		if (!input.Event.KeyEvent.bKeyDown) continue;
		ct = input.Event.KeyEvent.wRepeatCount;
		return input.Event.KeyEvent.wVirtualKeyCode;
	}
}

// ��������� ���������� ^�. ^Break, � �.�.

BOOL WINAPI ctrl(DWORD type)
{
	exit(0);
	return TRUE; // �� ���������� ������� ����
}

// ���������� ������ � �������� ������� ������

int getat(int x, int y)
{
	char c;
	DWORD res;
	COORD org = { x,y };
	// ����������� ������ � ������� �� ��� ���, ���� ��������� �� ����� ���������
	WaitForSingleObject(screenlock, INFINITE);
	ReadConsoleOutputCharacter(conout, &c, 1, org, &res);
	ReleaseMutex(screenlock); // unlock
	return c;
}

// ���������� ���� � ��������� ���� � ��������� ������� ���������� ����

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
		exit(0); // �� ������� �� ����������� ������
	}
	if ((hit + miss) % 20 == 0)
		InterlockedDecrement(&delayfactor); // ������ ���� ilock

}

char badchar[] = "-\\|/";

// ��� ����� ����������

void badguy(void *_y)
{
	int y = (int)_y; // ��������� ���������� �
	int dir;
	int x;
	// �������� � ���������� �����, ������ � ���������� ������
	x = y % 2 ? 0 : info.dwSize.X;
	// ���������� ����������� � ����������� �� ��������� �������
	dir = x ? -1 : 1;
	// ���� ��������� ��������� � �������� ������
	while ((dir == 1 && x != info.dwSize.X) || (dir == -1 && x != 0))
	{
		int dly;
		BOOL hitme = FALSE;
		// �������� �� ��������� (����?)
		if (getat(x, y) == '*') hitme = TRUE;
		// ����� ������� �� �����
		writeat(x, y, badchar[x % 4]);
		// ��� ���� �������� �� ���������
		if (getat(x, y) == '*') hitme = TRUE;
		// �������� �� ��������� ����� ���������
		//  ���������� �������
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
		// ��� ���� �������� �� ���������
		if (getat(x, y) == '*') hitme = TRUE;
		if (hitme)
		{
			// � ���������� ������!
			MessageBeep(-1);
			InterlockedIncrement(&hit);
			score();
			_endthread();
		}
		x += dir;
	}
	// ��������� ������!
	InterlockedIncrement(&miss);
	score();
}



// ���� ����� ���������� ��������� ������� �����������

void badguys(void *)

{
	// ���� ������� � ������ ���� � ������� 15 ������
	WaitForSingleObject(startevt, 15000);
	// ������� ���������� �����
	// ������ 5 ������ ���������� ���� �������
	// ���������� � ������������ �� 1 �� 10
	while (1)
	{
		if (random(0, 100)<(hit + miss) / 25 + 20)
			// �� �������� ����������� �������������
			_beginthread(badguy, 0, (void *)(random(1, 10)));
		Sleep(1000); // ������ �������
	}
}


// ��� ����� ����
// ������ ���� - ��� ��������� �����
void bullet(void *_xy_)
{
	COORD xy = *(COORD *)_xy_;
	if (getat(xy.X, xy.Y) == '*') return; // ����� ��� ���� ����
										  // ���� ���������
										  // ��������� �������
										  // ���� ������� ����� 0, �������� �� ����������
	if (WaitForSingleObject(bulletsem, 0) == WAIT_TIMEOUT) return;
	while (--xy.Y)
	{
		writeat(xy.X, xy.Y, '*'); // ���������� ����
		Sleep(100);
		writeat(xy.X, xy.Y, ' ');    // ������� ����
	}
	// ������� ������.- �������� 1 � ��������
	ReleaseSemaphore(bulletsem, 1, NULL);
}
// �������� ���������

void main()

{
	HANDLE me;
	// ��������� ���������� ����������
	conin = GetStdHandle(STD_INPUT_HANDLE);
	conout = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleCtrlHandler(ctrl, TRUE);
	SetConsoleMode(conin, ENABLE_WINDOW_INPUT);
	me = GetCurrentThread(); // �� �������� �������� ������������
							 // �������� ���������������� �� �������� ���������� �������� ������
	DuplicateHandle(GetCurrentProcess(), me, GetCurrentProcess(), &mainthread, 0, FALSE, DUPLICATE_SAME_ACCESS);
	startevt = CreateEvent(NULL, TRUE, FALSE, NULL);
	screenlock = CreateMutex(NULL, FALSE, NULL);
	InitializeCriticalSection(&gameover);
	bulletsem = CreateSemaphore(NULL, 3,3, NULL);
	GetConsoleScreenBufferInfo(conout, &info);
	// ���������������� ����������� ���������� �� �����
	score();
	// ��������� ��������� ��������� �����
	srand((unsigned)time(NULL));
	cls(); // �� ����� ���� �� �����
		   // ��������� ��������� ������� �����
	int y = info.dwSize.Y - 1;
	int x = info.dwSize.X / 2;
	// ��������� ����� badguys; ������ �� ������ �� ��� ���,
	// ���� �� ���������� ������� ��� ������� 15 ������
	_beginthread(badguys, 0, NULL); // �������� ���� ����
	while (1)
	{
		int c, ct;
		writeat(x, y, '|'); // ���������� �����
		c = getakey(ct);   // �������� ������
		switch (c)
		{
		case VK_SPACE:
		{
			static COORD xy;
			xy.X = x;
			xy.Y = y;
			_beginthread(bullet, 0, (void *)&xy);
			Sleep(100); // ���� ���� ����� ������� �� ��������� ����������
			break;
		}

		case VK_LEFT:  // ������� "�����!"

			SetEvent(startevt);    // ����� badguys ��������

			writeat(x, y, ' ');      // ������ � ������ �����

			while (ct--)        // �������������

				if (x) x--;

			break;

		case VK_RIGHT: // ������� "������!"; ������ �� ��

			SetEvent(startevt);

			writeat(x, y, ' ');

			while (ct--)

				if (x != info.dwSize.X - 1) x++;

			break;

		}
	}
}
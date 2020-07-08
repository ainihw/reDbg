#include <Windows.h>
#include <iostream>
#include <iomanip>
#include <stdio.h>
#include "resource.h"
#include "capstone.h"
#pragma comment(lib,"capstone.lib")

#pragma comment( lib, "comctl32.lib" )
#include <commctrl.h>
using namespace std;

#define	b00000000000000000000000100000000	0x100				//用来设置tf标志位
#define ORDER_NUM							40					//每次反汇编得指令数目
#define	CODE_NUM							200					//每次从目标进程读取机器码的数目

//int3断点结构
typedef struct Int3
{
	BYTE	oldByte;		//原始字节
	DWORD	dwAddress;		//断点地址
	Int3* lpNextINT3;
}INT3;


int		flag = 0;			//不同标志意义不同
int		flag1 = 0;			//标志是刚刚是否发生了int3断点。
char	Button;				//存储刚刚按下的按键
HWND	hWinMain;			//主窗口句柄
HWND	hChildWindow;		//子窗口句柄
BYTE	NewByte = 0xCC;		//断点处将被写入的字节
HANDLE  hDebugThread;		//调试线程的句柄

char	szAddress[]			= "地址";
char	szCode[]			= "机器码";
char	szDisassembled[]	= "反汇编代码";
char	szEsegesis[]		= "注释";
char	szRegister[]		= "寄存器";
char	szHex[]				= "HEX数据";
char	szAscii[]			= "ASCII";
char	szData[]			= "数据";
char	szRegisterData[]	= "值";
DWORD	_stdcall _GetEntryPoint(char* lpExeName);
void _stdcall _ShowContext(CONTEXT* lpstContext);
int _stdcall _Disassembled(int OrderNum, BYTE* lpBuffer, DWORD Address);
BOOL CALLBACK _MainDialog(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK _AboutDialog(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK _DisassembleDialog(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
int _stdcall _InitList(HWND hWnd, RECT stWindowRect);
int _stdcall _ChangeList(HWND hWnd, RECT stWindowRect);
int _stdcall _OpenFile();
int _stdcall _MyContinueDebugEvent(DEBUG_EVENT devent, PROCESS_INFORMATION pi);
int _stdcall _DealSingle(DEBUG_EVENT devent, INT3* lpHeadInt3, PROCESS_INFORMATION pi);
int _stdcall _DealBreakPoint(DEBUG_EVENT devent, INT3* lpHeadInt3, CONTEXT& stContext, DWORD dwEntryPoint, PROCESS_INFORMATION pi);




//UI主函数
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	MSG		stMsg;
	hWinMain = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, _MainDialog, NULL);




	while (GetMessage(&stMsg, NULL, 0, 0))
	{
		if (stMsg.message == WM_QUIT)
			break;
		TranslateMessage(&stMsg);
		DispatchMessage(&stMsg);
	}

	return stMsg.wParam;
}







//主窗口回调函数
BOOL CALLBACK _MainDialog(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	HICON	hIcon;
	
	switch (Msg)
	{

	case WM_COMMAND:
		switch (wParam)
		{
		//打开，创建调试线程
		case ID_40001:
			hDebugThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)_OpenFile, NULL, NULL, NULL);
			break;
		case ID_40007:
			CreateDialogParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG3), hWnd, _DisassembleDialog, 0);
			break;
		case ID_40003:
			SendMessage(hWnd, WM_CLOSE, 0, 0);
			break;
		case ID_40005:
			DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG2), hWnd, _AboutDialog, 0);
			break;
		}
		break;
	case WM_INITDIALOG:
		//这里不能用hWinMain，因为CreateDialogParam( )函数还没返回，其子在内进行窗口的注册和创建（从而发送WM_INITDIALOG消息）
		hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
		SendMessage(hWnd, WM_SETICON, ICON_BIG, LPARAM(hIcon));									
		break;
	case WM_CLOSE:
		DestroyWindow(hWnd);
		break;
	case WM_DESTROY:
		PostQuitMessage(NULL);
		break;
	default:
		return FALSE;
	}


	return TRUE;

}



//关于窗口回调函数
BOOL CALLBACK _AboutDialog(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HICON	hIcon;

	switch (message)
	{
	case WM_CLOSE:
		EndDialog(hWnd, 0);
		break;
	case WM_INITDIALOG:
		hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
		SendMessage(hWnd, WM_SETICON, ICON_BIG, LPARAM(hIcon));
		break;
	default:
		return FALSE;
	}
	return TRUE;

}


//反汇编窗口过程
BOOL CALLBACK _DisassembleDialog(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	RECT		stWindowRect;
	switch (message)
	{
	case WM_SIZE:
		GetClientRect(hWnd, &stWindowRect);
		MoveWindow(GetDlgItem(hWnd, IDC_LIST1), 0, 0, (stWindowRect.right / 12) * 9, (stWindowRect.bottom / 3) * 2, TRUE);
		MoveWindow(GetDlgItem(hWnd, IDC_LIST5), (stWindowRect.right / 12) * 9, 0, (stWindowRect.right / 12) * 3, (stWindowRect.bottom / 3) * 2, TRUE);
		MoveWindow(GetDlgItem(hWnd, IDC_LIST2), 0, (stWindowRect.bottom / 3) * 2, (stWindowRect.right / 12) * 9, stWindowRect.bottom / 3, TRUE);
		MoveWindow(GetDlgItem(hWnd, IDC_LIST3), (stWindowRect.right / 12) * 9, (stWindowRect.bottom / 3) * 2, (stWindowRect.right / 12) * 3, stWindowRect.bottom / 3, TRUE);
		_ChangeList(hWnd, stWindowRect);
		break;
	case WM_INITDIALOG:
		GetClientRect(hWnd, &stWindowRect);
		_InitList(hWnd, stWindowRect);
		break;
	case WM_CLOSE:
		EndDialog(hWnd, 0);
		break;
	

	default:
		return FALSE;
	}

	return TRUE;

}




//初始化列表头
int _stdcall _InitList(HWND hWnd, RECT	stWindowRect)
{
	LVCOLUMN    lvColumn;						//列表头属性
	memset(&lvColumn, 0, sizeof(lvColumn));
	
	
	//反汇编窗口
	SendMessage(GetDlgItem(hWnd, IDC_LIST1), LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)LVS_EX_FULLROWSELECT );
	lvColumn.mask = LVCF_TEXT | LVCF_WIDTH;
	
	lvColumn.cx = (stWindowRect.right / 36) * 3;
	lvColumn.pszText = szAddress;
	SendDlgItemMessage(hWnd, IDC_LIST1, LVM_INSERTCOLUMN, 0, (LPARAM)&lvColumn);

	
	lvColumn.cx = (stWindowRect.right / 36) * 6;
	lvColumn.pszText = szCode;
	SendDlgItemMessage(hWnd, IDC_LIST1, LVM_INSERTCOLUMN, 1, (LPARAM)&lvColumn);

	
	lvColumn.cx = (stWindowRect.right / 36) * 12;
	lvColumn.pszText = szDisassembled;
	SendDlgItemMessage(hWnd, IDC_LIST1, LVM_INSERTCOLUMN, 2, (LPARAM)&lvColumn);

	
	lvColumn.cx = (stWindowRect.right / 36) * 6;
	lvColumn.pszText = szEsegesis;
	SendDlgItemMessage(hWnd, IDC_LIST1, LVM_INSERTCOLUMN, 3, (LPARAM)&lvColumn);

	//寄存器窗口
	SendMessage(GetDlgItem(hWnd, IDC_LIST5), LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)LVS_EX_FULLROWSELECT);
	lvColumn.cx = (stWindowRect.right / 8);
	lvColumn.pszText = szRegister;
	SendDlgItemMessage(hWnd, IDC_LIST5, LVM_INSERTCOLUMN, 0, (LPARAM)&lvColumn);

	SendMessage(GetDlgItem(hWnd, IDC_LIST5), LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)LVS_EX_FULLROWSELECT);
	lvColumn.cx = (stWindowRect.right / 8);
	lvColumn.pszText = szRegisterData;
	SendDlgItemMessage(hWnd, IDC_LIST5, LVM_INSERTCOLUMN, 1, (LPARAM)&lvColumn);
	
	//数据窗口
	SendMessage(GetDlgItem(hWnd, IDC_LIST2), LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)LVS_EX_FULLROWSELECT);
	lvColumn.cx = (stWindowRect.right / 36) * 3;
	lvColumn.pszText = szAddress;
	SendDlgItemMessage(hWnd, IDC_LIST2, LVM_INSERTCOLUMN, 0, (LPARAM)&lvColumn);

	lvColumn.cx = (stWindowRect.right / 36) * 18;
	lvColumn.pszText = szHex;
	SendDlgItemMessage(hWnd, IDC_LIST2, LVM_INSERTCOLUMN, 1, (LPARAM)&lvColumn);

	lvColumn.cx = (stWindowRect.right / 36) * 6;
	lvColumn.pszText = szAscii;
	SendDlgItemMessage(hWnd, IDC_LIST2, LVM_INSERTCOLUMN, 2, (LPARAM)&lvColumn);


	//堆栈窗口
	SendMessage(GetDlgItem(hWnd, IDC_LIST3), LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)LVS_EX_FULLROWSELECT);
	lvColumn.cx = (stWindowRect.right / 12) * 1;
	lvColumn.pszText = szAddress;
	SendDlgItemMessage(hWnd, IDC_LIST3, LVM_INSERTCOLUMN, 0, (LPARAM)&lvColumn);

	lvColumn.cx = (stWindowRect.right / 12) * 1;
	lvColumn.pszText = szData;
	SendDlgItemMessage(hWnd, IDC_LIST3, LVM_INSERTCOLUMN, 1, (LPARAM)&lvColumn);


	return 0;
}



//改变列表头
int _stdcall _ChangeList(HWND hWnd, RECT stWindowRect)
{
	
	
	LVCOLUMN    lvColumn;						//ListView列表头属性
	memset(&lvColumn, 0, sizeof(lvColumn));

	
	//反汇编窗口
	lvColumn.mask = LVCF_TEXT | LVCF_WIDTH;
	lvColumn.cx = (stWindowRect.right / 36) * 3;
	lvColumn.pszText = szAddress;
	SendDlgItemMessage(hWnd, IDC_LIST1, LVM_SETCOLUMN, 0, (LPARAM)&lvColumn);

	
	lvColumn.cx = (stWindowRect.right / 36) * 6;
	lvColumn.pszText = szCode;
	SendDlgItemMessage(hWnd, IDC_LIST1, LVM_SETCOLUMN, 1, (LPARAM)&lvColumn);

	
	lvColumn.cx = (stWindowRect.right / 36) * 12;
	lvColumn.pszText = szDisassembled;
	SendDlgItemMessage(hWnd, IDC_LIST1, LVM_SETCOLUMN, 2, (LPARAM)&lvColumn);

	
	lvColumn.cx = (stWindowRect.right / 36) * 6;
	lvColumn.pszText = szEsegesis;
	SendDlgItemMessage(hWnd, IDC_LIST1, LVM_SETCOLUMN, 3, (LPARAM)&lvColumn);

	//寄存器窗口
	lvColumn.cx = (stWindowRect.right / 8);
	lvColumn.pszText = szRegister;
	SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETCOLUMN, 0, (LPARAM)&lvColumn);

	lvColumn.cx = (stWindowRect.right / 8);
	lvColumn.pszText = szRegisterData;
	SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETCOLUMN, 1, (LPARAM)&lvColumn);

	//数据窗口
	lvColumn.cx = (stWindowRect.right / 36) * 3;
	lvColumn.pszText = szAddress;
	SendDlgItemMessage(hWnd, IDC_LIST2, LVM_SETCOLUMN, 0, (LPARAM)&lvColumn);

	lvColumn.cx = (stWindowRect.right / 36) * 18;
	lvColumn.pszText = szHex;
	SendDlgItemMessage(hWnd, IDC_LIST2, LVM_SETCOLUMN, 1, (LPARAM)&lvColumn);

	lvColumn.cx = (stWindowRect.right / 36) * 6;
	lvColumn.pszText = szAscii;
	SendDlgItemMessage(hWnd, IDC_LIST2, LVM_SETCOLUMN, 2, (LPARAM)&lvColumn);


	//堆栈窗口
	lvColumn.cx = (stWindowRect.right / 12) * 1;
	lvColumn.pszText = szAddress;
	SendDlgItemMessage(hWnd, IDC_LIST3, LVM_SETCOLUMN, 0, (LPARAM)&lvColumn);

	lvColumn.cx = (stWindowRect.right / 12) * 1;
	lvColumn.pszText = szData;
	SendDlgItemMessage(hWnd, IDC_LIST3, LVM_SETCOLUMN, 1, (LPARAM)&lvColumn);

	return 0;
}




//调试线程
int _stdcall _OpenFile()
{
	char ExeName[MAX_PATH] = {0};			//可执行文件的路径

	OPENFILENAME		stOF;
	PROCESS_INFORMATION pi;					//接受新进程的一些有关信息
	STARTUPINFO			si;					//指定新进程的主窗体如何显示
	DEBUG_EVENT			devent;				//消息事件
	CONTEXT				stContext;			//线程信息块
	memset(&stOF, 0, sizeof(stOF));
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(STARTUPINFO);

	DWORD		dwEntryPoint;				//被调试程序的入口地址			
	INT3* lpHeadInt3 = NULL;			//断点链表


	//打开文件
	stOF.lStructSize = sizeof(stOF);
	stOF.hwndOwner = hWinMain;
	stOF.lpstrFilter = TEXT("可执行文件(*.exe,*.dll)\0*.exe;*.dll\0任意文件(*.*)\0*.*\0\0");
	stOF.lpstrFile = ExeName;
	stOF.nMaxFile = MAX_PATH;
	stOF.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

	if (!GetOpenFileName(&stOF))
	{
		MessageBox(NULL, TEXT("打开文件失败！"), NULL, MB_OK);
		return -1;
	}


	

	//创建被调试程序进程
	CreateProcess(
		ExeName,
		NULL,
		NULL,
		NULL,
		FALSE, // 不可继承
		DEBUG_ONLY_THIS_PROCESS | DEBUG_PROCESS, // 调试模式启动			(DEBUG_ONLY_THIS_PROCESS标志表示其不能调试进程如果被调试的话，此新进程不会成为其调试进程的调试对象)
		NULL,
		NULL,
		&si,
		&pi);

	
				
	//获取入口地址
	stContext.ContextFlags = CONTEXT_FULL;
	GetThreadContext(pi.hThread, &stContext);
	dwEntryPoint = stContext.Eax;



	
	//在程序入口处设下断点，从而使在程序入口处时中断到调试器中
	lpHeadInt3 = new INT3;
	lpHeadInt3->lpNextINT3 = NULL;
	lpHeadInt3->dwAddress = dwEntryPoint;
	ReadProcessMemory(pi.hProcess, (LPCVOID)dwEntryPoint, &lpHeadInt3->oldByte, 1, NULL);
	WriteProcessMemory(pi.hProcess, (LPVOID)dwEntryPoint, &NewByte, 1, NULL);		//将入口点的第一个字节改为0xCC

	while (TRUE)
	{
		WaitForDebugEvent(&devent, INFINITE);	//等待调试事件
		switch (devent.dwDebugEventCode)
		{
		case EXCEPTION_DEBUG_EVENT:				//异常调试事件
			switch (devent.u.Exception.ExceptionRecord.ExceptionCode)
			{
			case EXCEPTION_BREAKPOINT:			//断点异常

				_DealBreakPoint(devent, lpHeadInt3, stContext, dwEntryPoint, pi);							//处理断点异常
				break;
			case EXCEPTION_SINGLE_STEP:			//单步或者是硬件断点

				_DealSingle(devent, lpHeadInt3, pi);													//处理单步
				break;
			default:
				break;
			}
			break;
		}

		_MyContinueDebugEvent(devent, pi);

	}
	return 0;
}





//枚举子窗口句柄回调函数
BOOL CALLBACK _EnumChildProc(HWND hWnd, LPARAM lParam)
{
	char szBuffer[256] = { 0 };
	GetWindowText(hWnd, szBuffer, sizeof(szBuffer));
	if (lParam == 0)
		if (0 == strcmp(szBuffer, "CPU"))
		{
			hChildWindow = hWnd;
			return FALSE;
		}
	return TRUE;

}

//显示线程环境的一些值
void _stdcall _ShowContext(CONTEXT* lpstContext)
{
	HWND	hWnd;
	LVITEM  lvItem;												//ListView列表项属性
	char	szBuffer1[256] = { 0 };
	char	szBuffer2[256] = { 0 };
	EnumChildWindows(hWinMain, _EnumChildProc, 0);				//获得CPU的窗口句柄		
	hWnd = hChildWindow;
	int i= 0;
	while (i < 17)
	{
		
		memset(&lvItem, 0, sizeof(lvItem));
		lvItem.mask = LVIF_TEXT ;
		lvItem.iItem = i;
		SendDlgItemMessage(hWnd, IDC_LIST5, LVM_INSERTITEM, 0, (LPARAM)&lvItem);
		
		
		
		switch (i)
		{
		case 0:
			lstrcpy(szBuffer1, "EAX");
			wsprintf(szBuffer2, TEXT("%08lX"), lpstContext->Eax);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;
		case 1:
			lstrcpy(szBuffer1, "EBX");
			wsprintf(szBuffer2, TEXT("%08lX"), lpstContext->Ebx);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;
		case 2:
			lstrcpy(szBuffer1, "ECX");
			wsprintf(szBuffer2, TEXT("%08lX"), lpstContext->Ecx);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;
		case 3:
			lstrcpy(szBuffer1, "EDX");
			wsprintf(szBuffer2, TEXT("%08lX"), lpstContext->Edx);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;
		case 4:
			lstrcpy(szBuffer1, "EIP");
			wsprintf(szBuffer2, TEXT("%08lX"), lpstContext->Eip);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;
		case 5:
			lstrcpy(szBuffer1, "ESP");
			wsprintf(szBuffer2, TEXT("%08lX"), lpstContext->Esp);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;
		case 6:
			lstrcpy(szBuffer1, "EBP");
			wsprintf(szBuffer2, TEXT("%08lX"), lpstContext->Ebp);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;
		case 7:
			lstrcpy(szBuffer1, "ESI");
			wsprintf(szBuffer2, TEXT("%08lX"), lpstContext->Esi);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;
		case 8:
			lstrcpy(szBuffer1, "EDI");
			wsprintf(szBuffer2, TEXT("%08lX"), lpstContext->Edi);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;

		case 10:
			lstrcpy(szBuffer1, "CS");
			wsprintf(szBuffer2, TEXT("%04lX"), lpstContext->SegCs);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;
		case 11:
			lstrcpy(szBuffer1, "ES");
			wsprintf(szBuffer2, TEXT("%04lX"), lpstContext->SegEs);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;
		case 12:
			lstrcpy(szBuffer1, "FS");
			wsprintf(szBuffer2, TEXT("%04lX"), lpstContext->SegFs);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;
		case 13:
			lstrcpy(szBuffer1, "GS");
			wsprintf(szBuffer2, TEXT("%04lX"), lpstContext->SegGs);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;
		case 14:
			lstrcpy(szBuffer1, "SS");
			wsprintf(szBuffer2, TEXT("%04lX"), lpstContext->SegSs);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;

		case 16:
			lstrcpy(szBuffer1, "EFlags");
			wsprintf(szBuffer2, TEXT("%04lX"), lpstContext->EFlags);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
		}
		i++;
	}
}



//反汇编指令并显示出来
int _stdcall _Disassembled(int OrderNum, BYTE lpBuffer[], DWORD Address)
{

	csh			csHandle;				//用来调用Caps引擎API得句柄
	cs_insn*	lpInsn;					//用来保存反汇编指令得信息
	size_t		count;					//保存缓冲区中反汇编指令信息得大小
	HWND		hWnd;
	char		szBuffer1[256] = { 0 };
	char		szBuffer2[256] = { 0 };

	if (cs_open(CS_ARCH_X86, CS_MODE_32, &csHandle))
	{
		MessageBox(NULL, TEXT("反汇编失败！"), TEXT("错误"), MB_OK);
		return -1;
	}
	count = cs_disasm(csHandle, lpBuffer, CODE_NUM, Address, ORDER_NUM, &lpInsn);		//对缓冲区中的机器码进行反汇编





	EnumChildWindows(hWinMain, _EnumChildProc, 0);				//获得CPU的窗口句柄		
	hWnd = hChildWindow;
	LVITEM       lvItem;					//ListView列表项属性
	int			 i = 0;

	while (i < count)
	{
		memset(&lvItem, 0, sizeof(lvItem));
		lvItem.mask = LVIF_TEXT;
		lvItem.iItem = i;
		SendDlgItemMessage(hWnd, IDC_LIST1, LVM_INSERTITEM, 0, (LPARAM)&lvItem);


		wsprintf(szBuffer1, TEXT("%08lX"), lpInsn[i].address);
		lvItem.pszText = szBuffer1;
		lvItem.iSubItem = 0;
		SendDlgItemMessage(hWnd, IDC_LIST1, LVM_SETITEM, 0, (LPARAM)&lvItem);

		memset(szBuffer2, 0, sizeof(szBuffer2));
		for (int m = 0; m < lpInsn[i].size; m++)
		{
			wsprintf(szBuffer1, TEXT("%02X"), lpInsn[i].bytes[m]);
			lstrcat(szBuffer2, szBuffer1);
		}
		lvItem.pszText = szBuffer2;
		lvItem.iSubItem = 1;
		SendDlgItemMessage(hWnd, IDC_LIST1, LVM_SETITEM, 0, (LPARAM)&lvItem);


		wsprintf(szBuffer1, lpInsn[i].mnemonic);
		wsprintf(szBuffer2, lpInsn[i].op_str);
		lstrcat(szBuffer1, "   ");
		lstrcat(szBuffer1, szBuffer2);
		lvItem.pszText = szBuffer1;
		lvItem.iSubItem = 2;
		SendDlgItemMessage(hWnd, IDC_LIST1, LVM_SETITEM, 0, (LPARAM)&lvItem);

		i++;
	}


	cs_free(lpInsn, count);			//释放cs_disasm申请的内存
	cs_close(&csHandle);			//关闭句柄
	return 0;
}





int _stdcall _DealBreakPoint(DEBUG_EVENT devent, INT3* lpHeadInt3, CONTEXT& stContext, DWORD dwEntryPoint, PROCESS_INFORMATION pi)
{
	INT3* lpInt3 = lpHeadInt3;					//辅助头指针进行查询
	BYTE	    lpBuffer[256] = { 0 };			//存放待反汇编的机器码

	while (lpInt3 != NULL)		//判断是否是我们设置的断点
	{
		if (lpInt3->dwAddress == (DWORD)devent.u.Exception.ExceptionRecord.ExceptionAddress)
		{
			flag = 1;			//表示是我们设置的int3断点
			flag1 = 1;			//表示发生了int3断点
			break;
		}
		else
			lpInt3 = lpInt3->lpNextINT3;
	}
	if (flag == 0)				//如果不是我们设置的断点，则交给程序处理
		return -1;
	else						//如果是我们设置的断点则进行处理。
	{
		if (devent.u.Exception.dwFirstChance == 1)       //在第一次异常的时候处理
		{


			stContext.ContextFlags = CONTEXT_FULL;
			GetThreadContext(pi.hThread, &stContext);


			stContext.EFlags = stContext.EFlags | b00000000000000000000000100000000;			//设置TF单步标志位，以便还原断点
			stContext.Eip--;																	//令eip-1
			WriteProcessMemory(pi.hProcess, (LPVOID)stContext.Eip, &lpInt3->oldByte, 1, NULL);	//还原原来的字节
			SetThreadContext(pi.hThread, &stContext);
			_ShowContext(&stContext);															//显示所有寄存器的值



			if ((DWORD)devent.u.Exception.ExceptionRecord.ExceptionAddress == dwEntryPoint)		//如果是在入口点
			{
				ReadProcessMemory(pi.hProcess, (LPCVOID)dwEntryPoint, lpBuffer, CODE_NUM, NULL);
				_Disassembled(ORDER_NUM, lpBuffer, dwEntryPoint);										//进行机器码反汇编，并显示
			}
		}

	}

	return 0;

}





int _stdcall _DealSingle(DEBUG_EVENT devent, INT3* lpHeadInt3, PROCESS_INFORMATION pi)
{
	INT3* lpInt3 = lpHeadInt3;					
	BYTE		bBuffer;

	if (flag1 == 1)			//刚刚发生了int3断点（说明此单步是为了恢复刚刚触发的int3断点）
	{
		flag1 = 0;
		while (lpInt3 != NULL)
		{

			if (lpInt3->lpNextINT3 == NULL || lpInt3->lpNextINT3->dwAddress >= (DWORD)devent.u.Exception.ExceptionRecord.ExceptionAddress)
			{
				ReadProcessMemory(pi.hProcess, (LPVOID)lpInt3->dwAddress, &bBuffer, 1, NULL);
				if (bBuffer != 0xCC)
					WriteProcessMemory(pi.hProcess, (LPVOID)lpInt3->dwAddress, &NewByte, 1, NULL);
				break;
			}
			lpInt3 = lpInt3->lpNextINT3;
		}
		if (Button == 'k')
			flag = 2;			//直接运行操作
		else if (Button == 't')
			flag = 3;			//单步执行操作
	}
	else if (flag1 == 0)
		flag = 3;			//单步执行操作

	return 0;

}




int _stdcall _MyContinueDebugEvent(DEBUG_EVENT devent, PROCESS_INFORMATION pi)
{


	



	



	CONTEXT				stContext;			//线程信息块
	if (flag == 3)						//单步执行
	{
		GetThreadContext(pi.hThread, &stContext);
		_ShowContext(&stContext);
		SuspendThread(hDebugThread);

		if (Button == 'k')			//运行
			ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);
		else if (Button == 't')		//单步执行
		{
			stContext.EFlags = stContext.EFlags | b00000000000000000000000100000000;	//令TF位为1
			SetThreadContext(pi.hThread, &stContext);
			ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);
		}

	}
	else if (flag == 2)					//刚执行完int3断点处指令，利用单步执行恢复int3断点并直接运行
	{
		ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);
	}
	else if (flag == 1)					//程序执行到了断点处
	{

		SuspendThread(hDebugThread);
		ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);
	}
	else if (flag == 0)					//非异常调试事件以及程序真正的"异常"
		ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_EXCEPTION_NOT_HANDLED);
	flag = 0;


	return 0;
}
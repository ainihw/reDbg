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

#define	b00000000000000000000000100000000	0x100				//��������tf��־λ
#define ORDER_NUM							40					//ÿ�η�����ָ����Ŀ
#define	CODE_NUM							200					//ÿ�δ�Ŀ����̶�ȡ���������Ŀ

//int3�ϵ�ṹ
typedef struct Int3
{
	BYTE	oldByte;		//ԭʼ�ֽ�
	DWORD	dwAddress;		//�ϵ��ַ
	Int3* lpNextINT3;
}INT3;


int		flag = 0;			//��ͬ��־���岻ͬ
int		flag1 = 0;			//��־�Ǹո��Ƿ�����int3�ϵ㡣
char	Button;				//�洢�ոհ��µİ���
HWND	hWinMain;			//�����ھ��
HWND	hChildWindow;		//�Ӵ��ھ��
BYTE	NewByte = 0xCC;		//�ϵ㴦����д����ֽ�
HANDLE  hDebugThread;		//�����̵߳ľ��

char	szAddress[]			= "��ַ";
char	szCode[]			= "������";
char	szDisassembled[]	= "��������";
char	szEsegesis[]		= "ע��";
char	szRegister[]		= "�Ĵ���";
char	szHex[]				= "HEX����";
char	szAscii[]			= "ASCII";
char	szData[]			= "����";
char	szRegisterData[]	= "ֵ";
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




//UI������
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







//�����ڻص�����
BOOL CALLBACK _MainDialog(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	HICON	hIcon;
	
	switch (Msg)
	{

	case WM_COMMAND:
		switch (wParam)
		{
		//�򿪣����������߳�
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
		//���ﲻ����hWinMain����ΪCreateDialogParam( )������û���أ��������ڽ��д��ڵ�ע��ʹ������Ӷ�����WM_INITDIALOG��Ϣ��
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



//���ڴ��ڻص�����
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


//����ര�ڹ���
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




//��ʼ���б�ͷ
int _stdcall _InitList(HWND hWnd, RECT	stWindowRect)
{
	LVCOLUMN    lvColumn;						//�б�ͷ����
	memset(&lvColumn, 0, sizeof(lvColumn));
	
	
	//����ര��
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

	//�Ĵ�������
	SendMessage(GetDlgItem(hWnd, IDC_LIST5), LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)LVS_EX_FULLROWSELECT);
	lvColumn.cx = (stWindowRect.right / 8);
	lvColumn.pszText = szRegister;
	SendDlgItemMessage(hWnd, IDC_LIST5, LVM_INSERTCOLUMN, 0, (LPARAM)&lvColumn);

	SendMessage(GetDlgItem(hWnd, IDC_LIST5), LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)LVS_EX_FULLROWSELECT);
	lvColumn.cx = (stWindowRect.right / 8);
	lvColumn.pszText = szRegisterData;
	SendDlgItemMessage(hWnd, IDC_LIST5, LVM_INSERTCOLUMN, 1, (LPARAM)&lvColumn);
	
	//���ݴ���
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


	//��ջ����
	SendMessage(GetDlgItem(hWnd, IDC_LIST3), LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)LVS_EX_FULLROWSELECT);
	lvColumn.cx = (stWindowRect.right / 12) * 1;
	lvColumn.pszText = szAddress;
	SendDlgItemMessage(hWnd, IDC_LIST3, LVM_INSERTCOLUMN, 0, (LPARAM)&lvColumn);

	lvColumn.cx = (stWindowRect.right / 12) * 1;
	lvColumn.pszText = szData;
	SendDlgItemMessage(hWnd, IDC_LIST3, LVM_INSERTCOLUMN, 1, (LPARAM)&lvColumn);


	return 0;
}



//�ı��б�ͷ
int _stdcall _ChangeList(HWND hWnd, RECT stWindowRect)
{
	
	
	LVCOLUMN    lvColumn;						//ListView�б�ͷ����
	memset(&lvColumn, 0, sizeof(lvColumn));

	
	//����ര��
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

	//�Ĵ�������
	lvColumn.cx = (stWindowRect.right / 8);
	lvColumn.pszText = szRegister;
	SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETCOLUMN, 0, (LPARAM)&lvColumn);

	lvColumn.cx = (stWindowRect.right / 8);
	lvColumn.pszText = szRegisterData;
	SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETCOLUMN, 1, (LPARAM)&lvColumn);

	//���ݴ���
	lvColumn.cx = (stWindowRect.right / 36) * 3;
	lvColumn.pszText = szAddress;
	SendDlgItemMessage(hWnd, IDC_LIST2, LVM_SETCOLUMN, 0, (LPARAM)&lvColumn);

	lvColumn.cx = (stWindowRect.right / 36) * 18;
	lvColumn.pszText = szHex;
	SendDlgItemMessage(hWnd, IDC_LIST2, LVM_SETCOLUMN, 1, (LPARAM)&lvColumn);

	lvColumn.cx = (stWindowRect.right / 36) * 6;
	lvColumn.pszText = szAscii;
	SendDlgItemMessage(hWnd, IDC_LIST2, LVM_SETCOLUMN, 2, (LPARAM)&lvColumn);


	//��ջ����
	lvColumn.cx = (stWindowRect.right / 12) * 1;
	lvColumn.pszText = szAddress;
	SendDlgItemMessage(hWnd, IDC_LIST3, LVM_SETCOLUMN, 0, (LPARAM)&lvColumn);

	lvColumn.cx = (stWindowRect.right / 12) * 1;
	lvColumn.pszText = szData;
	SendDlgItemMessage(hWnd, IDC_LIST3, LVM_SETCOLUMN, 1, (LPARAM)&lvColumn);

	return 0;
}




//�����߳�
int _stdcall _OpenFile()
{
	char ExeName[MAX_PATH] = {0};			//��ִ���ļ���·��

	OPENFILENAME		stOF;
	PROCESS_INFORMATION pi;					//�����½��̵�һЩ�й���Ϣ
	STARTUPINFO			si;					//ָ���½��̵������������ʾ
	DEBUG_EVENT			devent;				//��Ϣ�¼�
	CONTEXT				stContext;			//�߳���Ϣ��
	memset(&stOF, 0, sizeof(stOF));
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(STARTUPINFO);

	DWORD		dwEntryPoint;				//�����Գ������ڵ�ַ			
	INT3* lpHeadInt3 = NULL;			//�ϵ�����


	//���ļ�
	stOF.lStructSize = sizeof(stOF);
	stOF.hwndOwner = hWinMain;
	stOF.lpstrFilter = TEXT("��ִ���ļ�(*.exe,*.dll)\0*.exe;*.dll\0�����ļ�(*.*)\0*.*\0\0");
	stOF.lpstrFile = ExeName;
	stOF.nMaxFile = MAX_PATH;
	stOF.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

	if (!GetOpenFileName(&stOF))
	{
		MessageBox(NULL, TEXT("���ļ�ʧ�ܣ�"), NULL, MB_OK);
		return -1;
	}


	

	//���������Գ������
	CreateProcess(
		ExeName,
		NULL,
		NULL,
		NULL,
		FALSE, // ���ɼ̳�
		DEBUG_ONLY_THIS_PROCESS | DEBUG_PROCESS, // ����ģʽ����			(DEBUG_ONLY_THIS_PROCESS��־��ʾ�䲻�ܵ��Խ�����������ԵĻ������½��̲����Ϊ����Խ��̵ĵ��Զ���)
		NULL,
		NULL,
		&si,
		&pi);

	
				
	//��ȡ��ڵ�ַ
	stContext.ContextFlags = CONTEXT_FULL;
	GetThreadContext(pi.hThread, &stContext);
	dwEntryPoint = stContext.Eax;



	
	//�ڳ�����ڴ����¶ϵ㣬�Ӷ�ʹ�ڳ�����ڴ�ʱ�жϵ���������
	lpHeadInt3 = new INT3;
	lpHeadInt3->lpNextINT3 = NULL;
	lpHeadInt3->dwAddress = dwEntryPoint;
	ReadProcessMemory(pi.hProcess, (LPCVOID)dwEntryPoint, &lpHeadInt3->oldByte, 1, NULL);
	WriteProcessMemory(pi.hProcess, (LPVOID)dwEntryPoint, &NewByte, 1, NULL);		//����ڵ�ĵ�һ���ֽڸ�Ϊ0xCC

	while (TRUE)
	{
		WaitForDebugEvent(&devent, INFINITE);	//�ȴ������¼�
		switch (devent.dwDebugEventCode)
		{
		case EXCEPTION_DEBUG_EVENT:				//�쳣�����¼�
			switch (devent.u.Exception.ExceptionRecord.ExceptionCode)
			{
			case EXCEPTION_BREAKPOINT:			//�ϵ��쳣

				_DealBreakPoint(devent, lpHeadInt3, stContext, dwEntryPoint, pi);							//����ϵ��쳣
				break;
			case EXCEPTION_SINGLE_STEP:			//����������Ӳ���ϵ�

				_DealSingle(devent, lpHeadInt3, pi);													//������
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





//ö���Ӵ��ھ���ص�����
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

//��ʾ�̻߳�����һЩֵ
void _stdcall _ShowContext(CONTEXT* lpstContext)
{
	HWND	hWnd;
	LVITEM  lvItem;												//ListView�б�������
	char	szBuffer1[256] = { 0 };
	char	szBuffer2[256] = { 0 };
	EnumChildWindows(hWinMain, _EnumChildProc, 0);				//���CPU�Ĵ��ھ��		
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



//�����ָ���ʾ����
int _stdcall _Disassembled(int OrderNum, BYTE lpBuffer[], DWORD Address)
{

	csh			csHandle;				//��������Caps����API�þ��
	cs_insn*	lpInsn;					//�������淴���ָ�����Ϣ
	size_t		count;					//���滺�����з����ָ����Ϣ�ô�С
	HWND		hWnd;
	char		szBuffer1[256] = { 0 };
	char		szBuffer2[256] = { 0 };

	if (cs_open(CS_ARCH_X86, CS_MODE_32, &csHandle))
	{
		MessageBox(NULL, TEXT("�����ʧ�ܣ�"), TEXT("����"), MB_OK);
		return -1;
	}
	count = cs_disasm(csHandle, lpBuffer, CODE_NUM, Address, ORDER_NUM, &lpInsn);		//�Ի������еĻ�������з����





	EnumChildWindows(hWinMain, _EnumChildProc, 0);				//���CPU�Ĵ��ھ��		
	hWnd = hChildWindow;
	LVITEM       lvItem;					//ListView�б�������
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


	cs_free(lpInsn, count);			//�ͷ�cs_disasm������ڴ�
	cs_close(&csHandle);			//�رվ��
	return 0;
}





int _stdcall _DealBreakPoint(DEBUG_EVENT devent, INT3* lpHeadInt3, CONTEXT& stContext, DWORD dwEntryPoint, PROCESS_INFORMATION pi)
{
	INT3* lpInt3 = lpHeadInt3;					//����ͷָ����в�ѯ
	BYTE	    lpBuffer[256] = { 0 };			//��Ŵ������Ļ�����

	while (lpInt3 != NULL)		//�ж��Ƿ����������õĶϵ�
	{
		if (lpInt3->dwAddress == (DWORD)devent.u.Exception.ExceptionRecord.ExceptionAddress)
		{
			flag = 1;			//��ʾ���������õ�int3�ϵ�
			flag1 = 1;			//��ʾ������int3�ϵ�
			break;
		}
		else
			lpInt3 = lpInt3->lpNextINT3;
	}
	if (flag == 0)				//��������������õĶϵ㣬�򽻸�������
		return -1;
	else						//������������õĶϵ�����д���
	{
		if (devent.u.Exception.dwFirstChance == 1)       //�ڵ�һ���쳣��ʱ����
		{


			stContext.ContextFlags = CONTEXT_FULL;
			GetThreadContext(pi.hThread, &stContext);


			stContext.EFlags = stContext.EFlags | b00000000000000000000000100000000;			//����TF������־λ���Ա㻹ԭ�ϵ�
			stContext.Eip--;																	//��eip-1
			WriteProcessMemory(pi.hProcess, (LPVOID)stContext.Eip, &lpInt3->oldByte, 1, NULL);	//��ԭԭ�����ֽ�
			SetThreadContext(pi.hThread, &stContext);
			_ShowContext(&stContext);															//��ʾ���мĴ�����ֵ



			if ((DWORD)devent.u.Exception.ExceptionRecord.ExceptionAddress == dwEntryPoint)		//���������ڵ�
			{
				ReadProcessMemory(pi.hProcess, (LPCVOID)dwEntryPoint, lpBuffer, CODE_NUM, NULL);
				_Disassembled(ORDER_NUM, lpBuffer, dwEntryPoint);										//���л����뷴��࣬����ʾ
			}
		}

	}

	return 0;

}





int _stdcall _DealSingle(DEBUG_EVENT devent, INT3* lpHeadInt3, PROCESS_INFORMATION pi)
{
	INT3* lpInt3 = lpHeadInt3;					
	BYTE		bBuffer;

	if (flag1 == 1)			//�ոշ�����int3�ϵ㣨˵���˵�����Ϊ�˻ָ��ոմ�����int3�ϵ㣩
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
			flag = 2;			//ֱ�����в���
		else if (Button == 't')
			flag = 3;			//����ִ�в���
	}
	else if (flag1 == 0)
		flag = 3;			//����ִ�в���

	return 0;

}




int _stdcall _MyContinueDebugEvent(DEBUG_EVENT devent, PROCESS_INFORMATION pi)
{


	



	



	CONTEXT				stContext;			//�߳���Ϣ��
	if (flag == 3)						//����ִ��
	{
		GetThreadContext(pi.hThread, &stContext);
		_ShowContext(&stContext);
		SuspendThread(hDebugThread);

		if (Button == 'k')			//����
			ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);
		else if (Button == 't')		//����ִ��
		{
			stContext.EFlags = stContext.EFlags | b00000000000000000000000100000000;	//��TFλΪ1
			SetThreadContext(pi.hThread, &stContext);
			ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);
		}

	}
	else if (flag == 2)					//��ִ����int3�ϵ㴦ָ����õ���ִ�лָ�int3�ϵ㲢ֱ������
	{
		ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);
	}
	else if (flag == 1)					//����ִ�е��˶ϵ㴦
	{

		SuspendThread(hDebugThread);
		ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);
	}
	else if (flag == 0)					//���쳣�����¼��Լ�����������"�쳣"
		ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_EXCEPTION_NOT_HANDLED);
	flag = 0;


	return 0;
}
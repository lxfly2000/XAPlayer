/*TODO: ׼�����Ӷ� .mp3 �ļ���֧�֡�
  ������ʹ�� v140_xp ƽ̨������򣬵����趨���ֲ����� xaudio2.h �ˣ�
  ���Ըó�����޷�֧��XP�ˡ�����Ȼ�þɰ� DirectX SDK ������ԣ�
  ���� Visual Studio 2015 ���뱾����
  ���ߣ�lxfly2000 http://www.baidu.com/p/lxfly2000
  �������BUG�뼰ʱ�ظ���*/
#include<Windows.h>
#include<xaudio2.h>
#include<CommCtrl.h>
#include"resource.h"
#include"SDKWaveFile.h"
#include"UseVisualStyle.h"

#pragma comment(lib,"xaudio2.lib")

INT_PTR CALLBACK DgCallback(HWND, UINT, WPARAM, LPARAM);

#define TEXT_PLAY	TEXT("����(&P)")
#define TEXT_PAUSE	TEXT("��ͣ(&P)")
#define TEXT_RESUME	TEXT("����(&P)")
#define XAPLAYER_MAX_VOLUME 100

BOOL OpenFileDialog(HWND, TCHAR*, TCHAR*);
void EndProgram(HWND);
bool StopAndReturn(HWND);
bool GetFileName(const TCHAR*, TCHAR*, const unsigned int, bool, bool);
void CALLBACK TimerProc(HWND, UINT, UINT_PTR, DWORD);
void CheckLoop(HWND, bool = false);
void CheckInfiniteLoop(HWND);
bool QueryGoOnIfFileExtDoesntMatch(HWND, const TCHAR*, const TCHAR*, bool);

IXAudio2 *xAudio2Engine = NULL;
IXAudio2MasteringVoice *masterVoice = NULL;
IXAudio2SourceVoice *srcVoice = NULL;
WAVEFORMATEX *waveformat = NULL;
static XAUDIO2_BUFFER buffer = { 0 };
static XAUDIO2_VOICE_STATE state;
static CWaveFile wav;
unsigned long wavsize = 0u;
unsigned char *wavdata = NULL;

int freshRate = 60;
static TCHAR appName[] = TEXT("XAPlayer");
TCHAR filepath[MAX_PATH] = TEXT("");
TCHAR filename[MAX_PATH] = TEXT("");
TCHAR textBuffer[270] = TEXT("TCHAR*: textBuffer");
TCHAR info[MAX_PATH] = TEXT("TCHAR*: info");
DEVMODE dm;
enum PlayStatus
{
	stop, play, pause
}playStatus;

int WINAPI wWinMain(HINSTANCE hI, HINSTANCE hPvI, PWSTR sParam, int iShow)
{
	dm.dmSize = sizeof(DEVMODE);
	EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm);
	freshRate = 1000 / dm.dmDisplayFrequency;
	//XAudio2 ��ʼ��
	playStatus = stop;
	buffer.Flags = XAUDIO2_END_OF_STREAM;
	buffer.LoopCount = 0;
	buffer.LoopBegin = 0;
	buffer.LoopLength = 0;
	buffer.PlayBegin = 0;
	if (FAILED(CoInitializeEx(0, COINIT_MULTITHREADED)))
	{
		MessageBox(NULL, TEXT("CoInitializeEx ʧ��"), TEXT("Error"), MB_ICONERROR);
		return 0;
	}
	if (FAILED(XAudio2Create(&xAudio2Engine)))
	{
		MessageBox(NULL, TEXT("δ�ܴ��� XAudio2 ���档"), TEXT("XAudio2"), MB_ICONERROR);
		return 0;
	}
	if (FAILED(xAudio2Engine->CreateMasteringVoice(&masterVoice)))
	{
		MessageBox(NULL, TEXT("δ�ܴ�����������"), TEXT("XAudio2 ������"), MB_ICONERROR);
		return 0;
	}
	if (lstrlen(sParam) > 0)
	{
		if (sParam[0] == TEXT('\"'))
		{
			lstrcpy(filepath, sParam + 1);
			filepath[lstrlen(filepath) - 1] = TEXT('\0');
		}
		else
		{
			lstrcpy(filepath, sParam);
		}
	}
	return (int)DialogBox(hI, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DgCallback);
}

INT_PTR CALLBACK DgCallback(HWND hwnd, UINT msg, WPARAM wP, LPARAM lP)
{
	switch (msg)
	{
	case WM_INITDIALOG:
		CheckLoop(hwnd);
		SetDlgItemInt(hwnd, IDC_EDIT_START, buffer.PlayBegin, FALSE);
		SetDlgItemInt(hwnd, IDC_EDIT_LOOP, buffer.LoopBegin, FALSE);
		SetDlgItemInt(hwnd, IDC_EDIT_END, buffer.LoopBegin + buffer.LoopLength, FALSE);
		SetDlgItemText(hwnd, IDC_STATIC_TIME, TEXT("δѡ���ļ���"));
		EnableWindow(GetDlgItem(hwnd, IDC_BUTTON_STOP), FALSE);
		/*SendMessage(GetDlgItem(hwnd, IDC_STATIC_TIME), WM_SETFONT,
			(WPARAM)CreateFont(12, 0, 0, 0,
				FW_NORMAL, FALSE, FALSE, FALSE,
				ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
				DEFAULT_PITCH | FF_SWISS, TEXT("SimSun")), 0);*/
		SendMessage(GetDlgItem(hwnd, IDC_SLIDER_VOLUME), TBM_SETRANGE, FALSE, MAKELPARAM(0, XAPLAYER_MAX_VOLUME));
		SendMessage(GetDlgItem(hwnd, IDC_SLIDER_VOLUME), TBM_SETPOS, TRUE, XAPLAYER_MAX_VOLUME);

		if (lstrlen(filepath) > 0)
		{
			GetFileName(filepath, textBuffer, MAX_PATH, true, true);
			if (!QueryGoOnIfFileExtDoesntMatch(hwnd, textBuffer, TEXT("WAV"), true))
				break;
			SetDlgItemText(hwnd, IDC_EDIT_FILENAME, filepath);
			SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDC_BUTTON_PLAY, 0), NULL);
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(wP))
		{
		case IDC_BUTTON_BROWSE:
			while (OpenFileDialog(hwnd, filepath, NULL))
			{
				GetFileName(filepath, textBuffer, MAX_PATH, true, true);
				if (QueryGoOnIfFileExtDoesntMatch(hwnd, textBuffer, TEXT("WAV"), true))
				{
					SetDlgItemText(hwnd, IDC_EDIT_FILENAME, filepath);
					break;
				}
			}
			break;
		case IDCANCEL:
			EndProgram(hwnd);
			break;
		case IDC_BUTTON_PLAY:
			switch (playStatus)
			{
			case stop:
				GetDlgItemText(hwnd, IDC_EDIT_FILENAME, filepath, MAX_PATH);
				GetFileName(filepath, filename, MAX_PATH, false, false);
				GetFileName(filename, textBuffer, MAX_PATH, true, true);

				if (filepath[0] == 0)
				{
					MessageBox(hwnd, TEXT("��ѡ���ļ���"), TEXT("��"), MB_ICONEXCLAMATION);
					break;
				}
				else if (FAILED(wav.Open(filepath, NULL, WAVEFILE_READ)))
				{
					MessageBox(hwnd, TEXT("�޷����Ÿ��ļ���������ѡ��"), TEXT("��"), MB_ICONEXCLAMATION);
					break;
				}
				buffer.LoopCount = GetDlgItemInt(hwnd, IDC_EDIT_LOOP_COUNT, NULL, FALSE);
				buffer.PlayBegin = GetDlgItemInt(hwnd, IDC_EDIT_START, NULL, FALSE);
				buffer.LoopBegin = GetDlgItemInt(hwnd, IDC_EDIT_LOOP, NULL, FALSE);
				buffer.LoopLength = GetDlgItemInt(hwnd, IDC_EDIT_END, NULL, FALSE);
				if (buffer.LoopCount)
				{
					if (buffer.LoopCount > XAUDIO2_LOOP_INFINITE)
					{
						MessageBeep(NULL);
						EDITBALLOONTIP tip = { sizeof(EDITBALLOONTIP), TEXT("ѭ��"),TEXT("ѭ���������ܳ��� 255."),TTI_ERROR_LARGE };
						Edit_ShowBalloonTip(GetDlgItem(hwnd, IDC_EDIT_LOOP_COUNT), &tip);
						break;
					}
					else if (buffer.LoopCount == XAUDIO2_LOOP_INFINITE)
						CheckInfiniteLoop(hwnd);
					else SendMessage(GetDlgItem(hwnd, IDC_CHECK_INFINITE), BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);
					if (buffer.LoopLength > 0 && buffer.PlayBegin >= buffer.LoopBegin + buffer.LoopLength)
					{
						MessageBeep(NULL);
						wsprintf(info, TEXT("������Ŀ�ʼλ�ó�����ѭ������λ�á�\n����ʼ (%u)��ѭ�� (%u)+���� (%u)��"),
							buffer.PlayBegin, buffer.LoopBegin, buffer.LoopLength);
						EDITBALLOONTIP tip = { sizeof(EDITBALLOONTIP),TEXT("��ʼλ��"),info,TTI_ERROR_LARGE };
						Edit_ShowBalloonTip(GetDlgItem(hwnd, IDC_EDIT_START), &tip);
						break;
					}
				}
				else
				{
					if (buffer.LoopBegin > 0)
					{
						EDITBALLOONTIP tip = { sizeof(EDITBALLOONTIP),TEXT("ѭ��λ��"),TEXT("ѭ������Ϊ 0 ʱ��ʹ�ø�ѡ�"),TTI_INFO_LARGE };
						Edit_ShowBalloonTip(GetDlgItem(hwnd, IDC_EDIT_LOOP), &tip);
					}
					else if (buffer.LoopLength > 0)
					{
						EDITBALLOONTIP tip = { sizeof(EDITBALLOONTIP),TEXT("ѭ������"),TEXT("ѭ������Ϊ 0 ʱ��ʹ�ø�ѡ�"),TTI_INFO_LARGE };
						Edit_ShowBalloonTip(GetDlgItem(hwnd, IDC_EDIT_END), &tip);
					}
					buffer.LoopBegin = buffer.LoopLength = 0;
					CheckLoop(hwnd);
				}
				waveformat = wav.GetFormat();
				wavsize = wav.GetSize();
				wavdata = new unsigned char[wavsize];
				if (SUCCEEDED(wav.Read(wavdata, wavsize, &wavsize)))
				{
					if (SUCCEEDED(xAudio2Engine->CreateSourceVoice(&srcVoice, waveformat)))
					{
						buffer.pAudioData = wavdata;
						buffer.AudioBytes = wavsize;
						if (SUCCEEDED(srcVoice->SubmitSourceBuffer(&buffer)))
						{
							srcVoice->Start();
							playStatus = play;
							SetDlgItemText(hwnd, IDC_BUTTON_PLAY, TEXT_PAUSE);
							wsprintf(textBuffer, TEXT("���ţ�%s"), filename, MAX_PATH);
							SetWindowText(hwnd, textBuffer);
							EnableWindow(GetDlgItem(hwnd, IDC_BUTTON_STOP), TRUE);
							SetTimer(hwnd, 0, freshRate, TimerProc);
						}
						else
						{
							MessageBox(hwnd, TEXT("ִ�� SubmitSourceBuffer ʱ����"), TEXT("XAudio2"), MB_ICONERROR);
						}
					}
				}
				break;
			case play:
				srcVoice->Stop();
				playStatus = pause;
				wsprintf(textBuffer, TEXT("��ͣ��%s"), filename, MAX_PATH);
				SetWindowText(hwnd, textBuffer);
				SetDlgItemText(hwnd, IDC_BUTTON_PLAY, TEXT_RESUME);
				//ע���ʽ���Ʒ��Ƿ���������Ͷ�Ӧ��http://blog.csdn.net/liuqz2009/article/details/6932822
				wsprintf(textBuffer, TEXT("�Ѳ��ţ�%10I64u smps\n��ͣ������%s"), state.SamplesPlayed, filename);
				SetDlgItemText(hwnd, IDC_STATIC_TIME, textBuffer);
				KillTimer(hwnd, 0);
				break;
			case pause:
				srcVoice->Start();
				playStatus = play;
				wsprintf(textBuffer, TEXT("���ţ�%s"), filename, MAX_PATH);
				SetWindowText(hwnd, textBuffer);
				SetDlgItemText(hwnd, IDC_BUTTON_PLAY, TEXT_PAUSE);
				SetTimer(hwnd, 0, freshRate, TimerProc);
				break;
			}
			break;
		case IDC_BUTTON_PAUSE:
			MessageBox(hwnd, TEXT("�������������ѭ������һ����Ƶ��\n"
				"Ҫʵ��ѭ�����ţ�ֻ�迪��ѭ��������������Ŀ���ɡ�\n"
				"��ʼλ�ã��������ʱ��ʼ��λ�ã�\nѭ��λ�ã�������λ�ú����ص�λ�ã�\n"
				"ѭ�����ȣ���ѭ��λ���𲥷ŵĳ��ȡ���0Ϊһֱ����Ƶ������\n"
				"������ֵ�ĵ�λ��Ϊ������ (Sample(s)).\n\n"
				"ע�⿪ʼλ�ú�ѭ��λ�ò��ܳ�����Ƶ����ĳ��ȣ�������ܻ����\n"
				"\t\t\t\t----�դ�դ�⤳��"), TEXT("����"), MB_ICONINFORMATION);
			break;
		case IDC_BUTTON_STOP:
			if (playStatus != stop)
				StopAndReturn(hwnd);
			break;
		case IDC_CHECK_LOOP:
			switch (IsDlgButtonChecked(hwnd, IDC_CHECK_LOOP))
			{
			case BST_CHECKED:
				/*buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
				SetDlgItemInt(hwnd, IDC_EDIT_LOOP_COUNT, XAUDIO2_LOOP_INFINITE, FALSE);
				SendMessage(GetDlgItem(hwnd, IDC_CHECK_INFINITE), BM_SETCHECK, (WPARAM)1, 0);*/
				CheckLoop(hwnd, true);
				break;
			case BST_UNCHECKED:
				/*buffer.LoopCount = 0;
				buffer.LoopBegin = 0;
				buffer.LoopLength = 0;
				buffer.PlayBegin = 0;*/
				CheckLoop(hwnd);
				break;
			default:
				break;
			}
			break;
		case IDC_CHECK_INFINITE:
			switch (IsDlgButtonChecked(hwnd, IDC_CHECK_INFINITE))
			{
			case BST_CHECKED:
				CheckInfiniteLoop(hwnd);
				break;
			case BST_UNCHECKED:
				if (buffer.LoopCount == 0)
					CheckLoop(hwnd);
				SetDlgItemInt(hwnd, IDC_EDIT_LOOP_COUNT, buffer.LoopCount, FALSE);
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
		break;
	case WM_DROPFILES:
		DragQueryFile((HDROP)wP, 0, filepath, MAX_PATH);
		DragFinish((HDROP)wP);
		GetFileName(filepath, textBuffer, MAX_PATH, true, true);
		if (!QueryGoOnIfFileExtDoesntMatch(hwnd, textBuffer, TEXT("WAV"), true))
			break;
		SetDlgItemText(hwnd, IDC_EDIT_FILENAME, filepath);
		break;
	case WM_SYSCOMMAND:
		switch (wP)
		{
		case SC_CLOSE:
			//�ڵ���ر�ʱϵͳ�ᷢ�� IDCANCEL �� WM_COMMAND ��Ϣ��
			//EndProgram(hwnd);
			break;
		default:
			break;
		}
		break;
	case WM_NOTIFY:
		switch (((LPNMHDR)lP)->code)
		{
		case NM_CUSTOMDRAW:
			switch (((LPNMHDR)lP)->idFrom)
			{
			case IDC_SLIDER_VOLUME:
				masterVoice->SetVolume((UINT)SendMessage(GetDlgItem(hwnd, IDC_SLIDER_VOLUME), TBM_GETPOS, 0, 0) / 100.0f);
				break;
			}
			break;
		}
		break;
	default:
		//�����Ҳ��µ�һ����ӡ��� = =||
		//DefWindowProc(hwnd, msg, wP, lP);
		break;
	}
	return 0;
}

void CALLBACK TimerProc(HWND hWnd, UINT nMsg, UINT_PTR nTimerid, DWORD dwTime)
{
	srcVoice->GetState(&state);
	//ע���ʽ���Ʒ��Ƿ���������Ͷ�Ӧ��http://blog.csdn.net/liuqz2009/article/details/6932822
	wsprintf(textBuffer, TEXT("�Ѳ��ţ�%10I64u smps\n���ڲ��ţ�%s"), state.SamplesPlayed, filename);
	SetDlgItemText(hWnd, IDC_STATIC_TIME, textBuffer);
	if (state.BuffersQueued == 0)
		StopAndReturn(hWnd);
}

BOOL OpenFileDialog(HWND hwnd, TCHAR *fpath, TCHAR *fname)
{
	OPENFILENAME openfile = { sizeof(OPENFILENAME) };
	openfile.lStructSize = sizeof(OPENFILENAME);
	openfile.hwndOwner = hwnd;
	openfile.hInstance = NULL;
	openfile.lpstrFilter = TEXT("������Ƶ (*.wav)\0*.wav\0���κ� MP3 ��ʽ��Ƶ (*.wav;*.mp3)\0*.wav;*.mp3\0MP3 ��ʽ��Ƶ���ݲ�֧�֣�\0*.mp3\0�����ļ�\0*\0\0");
	openfile.lpstrFile = fpath;
	openfile.lpstrTitle = TEXT("ѡ���ļ�");
	openfile.nMaxFile = MAX_PATH;
	openfile.lpstrFileTitle = fname;
	openfile.nMaxFileTitle = MAX_PATH;
	openfile.Flags = OFN_HIDEREADONLY;
	openfile.lpstrDefExt = TEXT("wav");
	return GetOpenFileName(&openfile);
}

void EndProgram(HWND hwnd)
{
	StopAndReturn(hwnd);
	masterVoice->DestroyVoice();
	xAudio2Engine->Release();
	CoUninitialize();
	PostQuitMessage(0);
}

bool StopAndReturn(HWND hwnd)
{
	if (playStatus != stop)
	{
		srcVoice->Stop();
		srcVoice->FlushSourceBuffers();
		playStatus = stop;
		if (srcVoice)
			srcVoice->DestroyVoice();
		if (wavdata)
		{
			delete[] wavdata;
			wavdata = nullptr;
		}
		SetDlgItemText(hwnd, IDC_BUTTON_PLAY, TEXT_PLAY);
		SetDlgItemText(hwnd, IDC_STATIC_TIME, TEXT("��ֹͣ��"));
		SetWindowText(hwnd, appName);
		EnableWindow(GetDlgItem(hwnd, IDC_BUTTON_STOP), FALSE);
		KillTimer(hwnd, 0);
		return true;
	}
	else return false;
}

//��� upperCase Ϊ�棬�� name �������ص��ַ����Ǵ�д�ġ�
bool GetFileName(const TCHAR *path, TCHAR *name, const unsigned int nsize, bool queryFileExt = false, bool upperCase = false)
{
	unsigned int l = lstrlen(path), i = 0, j = 0;
	if (l == 0) return false;
	for (i = l - 1; path[i - 1] != (queryFileExt ? TEXT('.') : TEXT('\\')) && i > 0; i--);
	if (nsize < l - i) return false;
	for (j = 0; j < l - i; j++)
		name[j] = path[j + i];
	name[j] = 0;
	if (upperCase)
		for (j = 0; j < l - i; j++)
			if (name[j] >= TEXT('a') && name[j] <= TEXT('z'))
				name[j] -= 32;
	return true;
}

void CheckLoop(HWND hwnd, bool checked)
{
	EnableWindow(GetDlgItem(hwnd, IDC_EDIT_LOOP_COUNT), (int)checked);
	SetDlgItemText(hwnd, IDC_STATIC_LOOP_COUNT, checked ? TEXT("ѭ������(&R)") : TEXT("ѭ��������"));
	if (checked)
	{
		SetDlgItemInt(hwnd, IDC_EDIT_LOOP_COUNT, buffer.LoopCount, TRUE);
		if (buffer.LoopCount == XAUDIO2_LOOP_INFINITE)
			SendMessage(GetDlgItem(hwnd, IDC_CHECK_INFINITE), BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
	}
	else
	{
		SetDlgItemInt(hwnd, IDC_EDIT_LOOP_COUNT, 0, FALSE);
		SendMessage(GetDlgItem(hwnd, IDC_CHECK_INFINITE), BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);
		SendMessage(GetDlgItem(hwnd, IDC_CHECK_LOOP), BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);
	}
}

void CheckInfiniteLoop(HWND hwnd)
{
	CheckLoop(hwnd, true);
	SetDlgItemInt(hwnd, IDC_EDIT_LOOP_COUNT, XAUDIO2_LOOP_INFINITE, FALSE);
	SendMessage(GetDlgItem(hwnd, IDC_CHECK_LOOP), BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
}

//��� query Ϊ��Ļ��Ͳ�ѯ�ʡ�
bool QueryGoOnIfFileExtDoesntMatch(HWND hwnd, const TCHAR *ext1, const TCHAR *ext2, bool query = true)
{
	if (lstrcmp(ext1, ext2))
	{
		if (query)
		{
			if (MessageBox(hwnd, TEXT("�ø�ʽ���ļ������޷����ţ��Ƿ�Ҫ������"), TEXT("ѡ���ļ�"), MB_YESNO | MB_ICONEXCLAMATION) == IDNO)
				return false;
		}
		else return false;
	}
	return true;
}
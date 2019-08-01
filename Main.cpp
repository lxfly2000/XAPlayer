/*TODO: 准备增加对 .mp3 文件的支持。
  本来想使用 v140_xp 平台编译程序，但是设定后发现不能用 xaudio2.h 了，
  所以该程序就无法支持XP了。（当然用旧版 DirectX SDK 或许可以）
  请用 Visual Studio 2015 编译本程序。
  作者：lxfly2000 http://www.baidu.com/p/lxfly2000
  有问题或BUG请及时回复。*/
#include<Windows.h>
#include<xaudio2.h>
#include<CommCtrl.h>
#include"resource.h"
#include"SDKWaveFile.h"
#include"UseVisualStyle.h"

#pragma comment(lib,"xaudio2.lib")

INT_PTR CALLBACK DgCallback(HWND, UINT, WPARAM, LPARAM);

#define TEXT_PLAY	TEXT("播放(&P)")
#define TEXT_PAUSE	TEXT("暂停(&P)")
#define TEXT_RESUME	TEXT("继续(&P)")
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
	//XAudio2 初始化
	playStatus = stop;
	buffer.Flags = XAUDIO2_END_OF_STREAM;
	buffer.LoopCount = 0;
	buffer.LoopBegin = 0;
	buffer.LoopLength = 0;
	buffer.PlayBegin = 0;
	if (FAILED(CoInitializeEx(0, COINIT_MULTITHREADED)))
	{
		MessageBox(NULL, TEXT("CoInitializeEx 失败"), TEXT("Error"), MB_ICONERROR);
		return 0;
	}
	if (FAILED(XAudio2Create(&xAudio2Engine)))
	{
		MessageBox(NULL, TEXT("未能创建 XAudio2 引擎。"), TEXT("XAudio2"), MB_ICONERROR);
		return 0;
	}
	if (FAILED(xAudio2Engine->CreateMasteringVoice(&masterVoice)))
	{
		MessageBox(NULL, TEXT("未能创建主声音。"), TEXT("XAudio2 主声音"), MB_ICONERROR);
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
		SetDlgItemText(hwnd, IDC_STATIC_TIME, TEXT("未选择文件。"));
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
					MessageBox(hwnd, TEXT("请选择文件。"), TEXT("打开"), MB_ICONEXCLAMATION);
					break;
				}
				else if (FAILED(wav.Open(filepath, NULL, WAVEFILE_READ)))
				{
					MessageBox(hwnd, TEXT("无法播放该文件，请重新选择。"), TEXT("打开"), MB_ICONEXCLAMATION);
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
						EDITBALLOONTIP tip = { sizeof(EDITBALLOONTIP), TEXT("循环"),TEXT("循环次数不能超过 255."),TTI_ERROR_LARGE };
						Edit_ShowBalloonTip(GetDlgItem(hwnd, IDC_EDIT_LOOP_COUNT), &tip);
						break;
					}
					else if (buffer.LoopCount == XAUDIO2_LOOP_INFINITE)
						CheckInfiniteLoop(hwnd);
					else SendMessage(GetDlgItem(hwnd, IDC_CHECK_INFINITE), BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);
					if (buffer.LoopLength > 0 && buffer.PlayBegin >= buffer.LoopBegin + buffer.LoopLength)
					{
						MessageBeep(NULL);
						wsprintf(info, TEXT("你输入的开始位置超过了循环结束位置。\n（开始 (%u)≥循环 (%u)+长度 (%u)）"),
							buffer.PlayBegin, buffer.LoopBegin, buffer.LoopLength);
						EDITBALLOONTIP tip = { sizeof(EDITBALLOONTIP),TEXT("开始位置"),info,TTI_ERROR_LARGE };
						Edit_ShowBalloonTip(GetDlgItem(hwnd, IDC_EDIT_START), &tip);
						break;
					}
				}
				else
				{
					if (buffer.LoopBegin > 0)
					{
						EDITBALLOONTIP tip = { sizeof(EDITBALLOONTIP),TEXT("循环位置"),TEXT("循环次数为 0 时不使用该选项。"),TTI_INFO_LARGE };
						Edit_ShowBalloonTip(GetDlgItem(hwnd, IDC_EDIT_LOOP), &tip);
					}
					else if (buffer.LoopLength > 0)
					{
						EDITBALLOONTIP tip = { sizeof(EDITBALLOONTIP),TEXT("循环长度"),TEXT("循环次数为 0 时不使用该选项。"),TTI_INFO_LARGE };
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
							wsprintf(textBuffer, TEXT("播放：%s"), filename, MAX_PATH);
							SetWindowText(hwnd, textBuffer);
							EnableWindow(GetDlgItem(hwnd, IDC_BUTTON_STOP), TRUE);
							SetTimer(hwnd, 0, freshRate, TimerProc);
						}
						else
						{
							MessageBox(hwnd, TEXT("执行 SubmitSourceBuffer 时出错。"), TEXT("XAudio2"), MB_ICONERROR);
						}
					}
				}
				break;
			case play:
				srcVoice->Stop();
				playStatus = pause;
				wsprintf(textBuffer, TEXT("暂停：%s"), filename, MAX_PATH);
				SetWindowText(hwnd, textBuffer);
				SetDlgItemText(hwnd, IDC_BUTTON_PLAY, TEXT_RESUME);
				//注意格式控制符是否与变量类型对应。http://blog.csdn.net/liuqz2009/article/details/6932822
				wsprintf(textBuffer, TEXT("已播放：%10I64u smps\n暂停：　　%s"), state.SamplesPlayed, filename);
				SetDlgItemText(hwnd, IDC_STATIC_TIME, textBuffer);
				KillTimer(hwnd, 0);
				break;
			case pause:
				srcVoice->Start();
				playStatus = play;
				wsprintf(textBuffer, TEXT("播放：%s"), filename, MAX_PATH);
				SetWindowText(hwnd, textBuffer);
				SetDlgItemText(hwnd, IDC_BUTTON_PLAY, TEXT_PAUSE);
				SetTimer(hwnd, 0, freshRate, TimerProc);
				break;
			}
			break;
		case IDC_BUTTON_PAUSE:
			MessageBox(hwnd, TEXT("本程序可以用来循环播放一个音频。\n"
				"要实现循环播放，只需开启循环并设置以下项目即可。\n"
				"开始位置：点击播放时开始的位置；\n循环位置：到结束位置后跳回的位置；\n"
				"循环长度：从循环位置起播放的长度。（0为一直到音频结束）\n"
				"以上数值的单位均为采样点 (Sample(s)).\n\n"
				"注意开始位置和循环位置不能超过音频本身的长度，否则可能会出错。\n"
				"\t\t\t\t----ふわふわもこう"), TEXT("帮助"), MB_ICONINFORMATION);
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
			//在点击关闭时系统会发送 IDCANCEL 的 WM_COMMAND 消息。
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
		//这是我踩下的一个大坑…… = =||
		//DefWindowProc(hwnd, msg, wP, lP);
		break;
	}
	return 0;
}

void CALLBACK TimerProc(HWND hWnd, UINT nMsg, UINT_PTR nTimerid, DWORD dwTime)
{
	srcVoice->GetState(&state);
	//注意格式控制符是否与变量类型对应。http://blog.csdn.net/liuqz2009/article/details/6932822
	wsprintf(textBuffer, TEXT("已播放：%10I64u smps\n正在播放：%s"), state.SamplesPlayed, filename);
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
	openfile.lpstrFilter = TEXT("波形音频 (*.wav)\0*.wav\0波形和 MP3 格式音频 (*.wav;*.mp3)\0*.wav;*.mp3\0MP3 格式音频（暂不支持）\0*.mp3\0所有文件\0*\0\0");
	openfile.lpstrFile = fpath;
	openfile.lpstrTitle = TEXT("选择文件");
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
		SetDlgItemText(hwnd, IDC_STATIC_TIME, TEXT("已停止。"));
		SetWindowText(hwnd, appName);
		EnableWindow(GetDlgItem(hwnd, IDC_BUTTON_STOP), FALSE);
		KillTimer(hwnd, 0);
		return true;
	}
	else return false;
}

//如果 upperCase 为真，则 name 变量返回的字符串是大写的。
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
	SetDlgItemText(hwnd, IDC_STATIC_LOOP_COUNT, checked ? TEXT("循环次数(&R)") : TEXT("循环次数："));
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

//如果 query 为否的话就不询问。
bool QueryGoOnIfFileExtDoesntMatch(HWND hwnd, const TCHAR *ext1, const TCHAR *ext2, bool query = true)
{
	if (lstrcmp(ext1, ext2))
	{
		if (query)
		{
			if (MessageBox(hwnd, TEXT("该格式的文件可能无法播放，是否要继续？"), TEXT("选择文件"), MB_YESNO | MB_ICONEXCLAMATION) == IDNO)
				return false;
		}
		else return false;
	}
	return true;
}
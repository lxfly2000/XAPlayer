/*TODO: 彈姥奐紗斤 .mp3 猟周議屶隔。
  云栖�詈荒� v140_xp 峠岬園咎殻會��徽頁譜協朔窟�峅残樮� xaudio2.h 阻��
  侭參乎殻會祥涙隈屶隔XP阻。�┻曳屍綻桧� DirectX SDK 賜俯辛參��
  萩喘 Visual Studio 2015 園咎云殻會。
  恬宀��lxfly2000 http://www.baidu.com/p/lxfly2000
  嗤諒籾賜BUG萩式扮指鹸。*/
#include<Windows.h>
#include<xaudio2.h>
#include<CommCtrl.h>
#include"resource.h"
#include"SDKWaveFile.h"
#include"UseVisualStyle.h"

#pragma comment(lib,"xaudio2.lib")

INT_PTR CALLBACK DgCallback(HWND, UINT, WPARAM, LPARAM);

#define TEXT_PLAY	TEXT("殴慧(&P)")
#define TEXT_PAUSE	TEXT("壙唯(&P)")
#define TEXT_RESUME	TEXT("写偬(&P)")
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
	//XAudio2 兜兵晒
	playStatus = stop;
	buffer.Flags = XAUDIO2_END_OF_STREAM;
	buffer.LoopCount = 0;
	buffer.LoopBegin = 0;
	buffer.LoopLength = 0;
	buffer.PlayBegin = 0;
	if (FAILED(CoInitializeEx(0, COINIT_MULTITHREADED)))
	{
		MessageBox(NULL, TEXT("CoInitializeEx 払移"), TEXT("Error"), MB_ICONERROR);
		return 0;
	}
	if (FAILED(XAudio2Create(&xAudio2Engine)))
	{
		MessageBox(NULL, TEXT("隆嬬幹秀 XAudio2 哈陪。"), TEXT("XAudio2"), MB_ICONERROR);
		return 0;
	}
	if (FAILED(xAudio2Engine->CreateMasteringVoice(&masterVoice)))
	{
		MessageBox(NULL, TEXT("隆嬬幹秀麼蕗咄。"), TEXT("XAudio2 麼蕗咄"), MB_ICONERROR);
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
		SetDlgItemText(hwnd, IDC_STATIC_TIME, TEXT("隆僉夲猟周。"));
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
					MessageBox(hwnd, TEXT("萩僉夲猟周。"), TEXT("嬉蝕"), MB_ICONEXCLAMATION);
					break;
				}
				else if (FAILED(wav.Open(filepath, NULL, WAVEFILE_READ)))
				{
					MessageBox(hwnd, TEXT("涙隈殴慧乎猟周��萩嶷仟僉夲。"), TEXT("嬉蝕"), MB_ICONEXCLAMATION);
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
						EDITBALLOONTIP tip = { sizeof(EDITBALLOONTIP), TEXT("儉桟"),TEXT("儉桟肝方音嬬階狛 255."),TTI_ERROR_LARGE };
						Edit_ShowBalloonTip(GetDlgItem(hwnd, IDC_EDIT_LOOP_COUNT), &tip);
						break;
					}
					else if (buffer.LoopCount == XAUDIO2_LOOP_INFINITE)
						CheckInfiniteLoop(hwnd);
					else SendMessage(GetDlgItem(hwnd, IDC_CHECK_INFINITE), BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);
					if (buffer.LoopLength > 0 && buffer.PlayBegin >= buffer.LoopBegin + buffer.LoopLength)
					{
						MessageBeep(NULL);
						wsprintf(info, TEXT("低補秘議蝕兵了崔階狛阻儉桟潤崩了崔。\n�┸�兵 (%u)−儉桟 (%u)+海業 (%u)��"),
							buffer.PlayBegin, buffer.LoopBegin, buffer.LoopLength);
						EDITBALLOONTIP tip = { sizeof(EDITBALLOONTIP),TEXT("蝕兵了崔"),info,TTI_ERROR_LARGE };
						Edit_ShowBalloonTip(GetDlgItem(hwnd, IDC_EDIT_START), &tip);
						break;
					}
				}
				else
				{
					if (buffer.LoopBegin > 0)
					{
						EDITBALLOONTIP tip = { sizeof(EDITBALLOONTIP),TEXT("儉桟了崔"),TEXT("儉桟肝方葎 0 扮音聞喘乎僉�遏�"),TTI_INFO_LARGE };
						Edit_ShowBalloonTip(GetDlgItem(hwnd, IDC_EDIT_LOOP), &tip);
					}
					else if (buffer.LoopLength > 0)
					{
						EDITBALLOONTIP tip = { sizeof(EDITBALLOONTIP),TEXT("儉桟海業"),TEXT("儉桟肝方葎 0 扮音聞喘乎僉�遏�"),TTI_INFO_LARGE };
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
							wsprintf(textBuffer, TEXT("殴慧��%s"), filename, MAX_PATH);
							SetWindowText(hwnd, textBuffer);
							EnableWindow(GetDlgItem(hwnd, IDC_BUTTON_STOP), TRUE);
							SetTimer(hwnd, 0, freshRate, TimerProc);
						}
						else
						{
							MessageBox(hwnd, TEXT("峇佩 SubmitSourceBuffer 扮竃危。"), TEXT("XAudio2"), MB_ICONERROR);
						}
					}
				}
				break;
			case play:
				srcVoice->Stop();
				playStatus = pause;
				wsprintf(textBuffer, TEXT("壙唯��%s"), filename, MAX_PATH);
				SetWindowText(hwnd, textBuffer);
				SetDlgItemText(hwnd, IDC_BUTTON_PLAY, TEXT_RESUME);
				//廣吭鯉塀陣崙憲頁倦嚥延楚窃侏斤哘。http://blog.csdn.net/liuqz2009/article/details/6932822
				wsprintf(textBuffer, TEXT("厮殴慧��%10I64u smps\n壙唯�此　�%s"), state.SamplesPlayed, filename);
				SetDlgItemText(hwnd, IDC_STATIC_TIME, textBuffer);
				KillTimer(hwnd, 0);
				break;
			case pause:
				srcVoice->Start();
				playStatus = play;
				wsprintf(textBuffer, TEXT("殴慧��%s"), filename, MAX_PATH);
				SetWindowText(hwnd, textBuffer);
				SetDlgItemText(hwnd, IDC_BUTTON_PLAY, TEXT_PAUSE);
				SetTimer(hwnd, 0, freshRate, TimerProc);
				break;
			}
			break;
		case IDC_BUTTON_PAUSE:
			MessageBox(hwnd, TEXT("云殻會辛參喘栖儉桟殴慧匯倖咄撞。\n"
				"勣糞�嶂�桟殴慧��峪俶蝕尼儉桟旺譜崔參和�酊深歓鼻�\n"
				"蝕兵了崔�叉禹�殴慧扮蝕兵議了崔��\n儉桟了崔�叉十疂�了崔朔柳指議了崔��\n"
				"儉桟海業�佐嗔�桟了崔軟殴慧議海業。��0葎匯岷欺咄撞潤崩��\n"
				"參貧方峙議汽了譲葎寡劔泣 (Sample(s)).\n\n"
				"廣吭蝕兵了崔才儉桟了崔音嬬階狛咄撞云附議海業��倦夸辛嬬氏竃危。\n"
				"\t\t\t\t----ふわふわもこう"), TEXT("逸廁"), MB_ICONINFORMATION);
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
			//壓泣似購液扮狼由氏窟僕 IDCANCEL 議 WM_COMMAND ��連。
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
		//宸頁厘家和議匯倖寄甚´´ = =||
		//DefWindowProc(hwnd, msg, wP, lP);
		break;
	}
	return 0;
}

void CALLBACK TimerProc(HWND hWnd, UINT nMsg, UINT_PTR nTimerid, DWORD dwTime)
{
	srcVoice->GetState(&state);
	//廣吭鯉塀陣崙憲頁倦嚥延楚窃侏斤哘。http://blog.csdn.net/liuqz2009/article/details/6932822
	wsprintf(textBuffer, TEXT("厮殴慧��%10I64u smps\n屎壓殴慧��%s"), state.SamplesPlayed, filename);
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
	openfile.lpstrFilter = TEXT("襖侘咄撞 (*.wav)\0*.wav\0襖侘才 MP3 鯉塀咄撞 (*.wav;*.mp3)\0*.wav;*.mp3\0MP3 鯉塀咄撞��壙音屶隔��\0*.mp3\0侭嗤猟周\0*\0\0");
	openfile.lpstrFile = fpath;
	openfile.lpstrTitle = TEXT("僉夲猟周");
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
		SetDlgItemText(hwnd, IDC_STATIC_TIME, TEXT("厮唯峭。"));
		SetWindowText(hwnd, appName);
		EnableWindow(GetDlgItem(hwnd, IDC_BUTTON_STOP), FALSE);
		KillTimer(hwnd, 0);
		return true;
	}
	else return false;
}

//泌惚 upperCase 葎寔��夸 name 延楚卦指議忖憲堪頁寄亟議。
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
	SetDlgItemText(hwnd, IDC_STATIC_LOOP_COUNT, checked ? TEXT("儉桟肝方(&R)") : TEXT("儉桟肝方��"));
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

//泌惚 query 葎倦議三祥音儂諒。
bool QueryGoOnIfFileExtDoesntMatch(HWND hwnd, const TCHAR *ext1, const TCHAR *ext2, bool query = true)
{
	if (lstrcmp(ext1, ext2))
	{
		if (query)
		{
			if (MessageBox(hwnd, TEXT("乎鯉塀議猟周辛嬬涙隈殴慧��頁倦勣写偬��"), TEXT("僉夲猟周"), MB_YESNO | MB_ICONEXCLAMATION) == IDNO)
				return false;
		}
		else return false;
	}
	return true;
}
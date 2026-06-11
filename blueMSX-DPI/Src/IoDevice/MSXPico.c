#include "MediaDb.h"
#include "DeviceManager.h"
#include "DebugDeviceManager.h"
#include "SaveState.h"
#include "Board.h"
#include "IoPort.h"
#include "ArchMidi.h"
#include "Language.h"
#include "MSXPico.h"
#include <stdlib.h>

#include <stdio.h>
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#pragma comment(lib,"winmm.lib")
#include "mmreg.h"
#include "Vfw.h"

typedef struct {
	int ioState; // 0 = stopped, 1 = playing, 2=paused
	int loop;
} MSXPico;

static UInt8 readIo(MSXPico* msxPico, UInt16 ioPort);
static void writeIo(MSXPico* msxPico, UInt16 ioPort, UInt8 value);
static void fuckMe(MSXPico* msxPico, UInt16 ioPort, UInt8 value);
static void destroyAll();
static MCIERROR sendMCI(char *command);
static void stopMusic();
static void playMusic();
static void spitError(char *text, MCIERROR mcie);

MSXPico* msxPico;
static HWND _hwnd;
static int loop;
static int playing;
static char filename[1024];
static char filePath[1024];
static 	char loopName[1024];
static int destroyed;
static MCIDEVICEID mciMusicId;
static MCIDEVICEID mciSampleId;
static int  lastMusic;

static int kaboom;
static int playSong;
static int stopSong;
static int playSample;
static int pauseSong;


void MSXPicoSetPath(char *path) {
	OutputDebugString("************* SETTING MSXPICOWAVE FILE PATH\n");
	OutputDebugString(path);
	OutputDebugString("\n************* SETTING MSXPICOWAVE FILE PATH\n");
	char drive[16];
	memset(drive, 0, sizeof(drive));
	char folder[2048];

	memset(folder, 0, sizeof(folder));
	_splitpath_s(path,
		drive, sizeof(drive),             // Don't need drive
		folder, sizeof(folder),    // Just the directory
		NULL, 0,             // Don't need filename
		NULL, 0);

	memset(filePath, 0, sizeof(filePath));
	strcpy(filePath, drive);
	strcat(filePath, folder);

	OutputDebugString("\n***********************");
	OutputDebugString(filePath);
	OutputDebugString("***********************\n");
	//memset(filePath, 0, sizeof(filePath));
	//strcpy(dir, path);
}

void MSXPicoSetHWND(HWND hwnd) {
	OutputDebugString("************* SETTING MSXPICOWAVE HWND");
	_hwnd = hwnd;
}

void MSXPicoNotify() {
	OutputDebugString("******* MCI NOTIFICATION!!\n");
	//MSXPicoMusicEnded();
	//ioPortWrite(msxPico, 0x92, 0xFF);
	if (loop) {
		playSong = TRUE;
	}
	playing = 0;
}

void MSXPicoMusicEnded() {
/*
	if (destroyed != 0) return;
	if (playing == 0) return;

	OutputDebugString("\n\n**********************************************\n");
	OutputDebugString("** MUSIC ENDED, TRYING TO LOOP IT...        **\n");
	OutputDebugString("**********************************************\n\n");

	if (loop == 1) {
		//playMusic();

		// achei que falsificar uma escrita na porta ia resolver...
		ioPortWrite(msxPico, 0x92, 0);
		ioPortWrite(msxPico, 0x92, lastMusic | 128);
	}
*/
}

/*
static void replay() {
	OutputDebugString("****** music ended...");
	if (loop == 1) {
		OutputDebugString("****** trying to play it again...");
		sendMCI("close MSXWAVE");
		sendMCI(filename);
		//retval = sendMCI("setaudio MSXWAVE volume to 10");
		sendMCI("play MSXWAVE notify");
	}
	else {
		playing = 0;
	}
}
*/

static void printout(int value) {
	char message[63];
	sprintf(message, "%d", value);
	OutputDebugString(message);
}

DWORD WINAPI ThreadFunc(void* data) {
	// Do stuff.  This will be the first function called on the new thread.
	// When this function returns, the thread goes away.  See MSDN for more details.
	while (kaboom == FALSE) {
		if (stopSong == TRUE) {
			if (playing==1) { 
				OutputDebugString("\n **** ::stopMusic:: TRYING TO STOP MUSIC mciId: "); printout(mciMusicId);
				MCI_GENERIC_PARMS mciGenericParams;
				mciGenericParams.dwCallback = 0; // (DWORD)_hwnd;
				DWORD flags = 0; // MCI_WAIT;
				MCIERROR err = mciSendCommand(mciMusicId, MCI_STOP, flags, (DWORD)(LPVOID)&mciGenericParams);
				if (err != 0) spitError("\n ::stopMusic:: ERROR STOPPING MUSIC!", err);
				stopSong = FALSE;
				playing = 0;
			}

			if (mciSampleId != 0) {
				OutputDebugString("\n **** ::stopMusic:: TRYING TO STOP SAMPLE mciId: "); printout(mciSampleId);
				MCI_GENERIC_PARMS mciGenericParams;
				mciGenericParams.dwCallback = 0; // (DWORD)_hwnd;
				DWORD flags = MCI_WAIT;
				MCIERROR err = mciSendCommand(mciSampleId, MCI_STOP, flags, (DWORD)(LPVOID)&mciGenericParams);
				if (err != 0) spitError("\n ::stopMusic:: ERROR STOPPING SAMPLE!", err);
				err = mciSendCommand(mciSampleId, MCI_CLOSE, flags, (DWORD)(LPVOID)&mciGenericParams);
				if (err != 0) spitError("\n ::stopMusic:: ERROR CLOSING SAMPLE!", err);

				stopSong = FALSE;
				mciSampleId = -1;
			}

		} else {
			if (playSong == TRUE) {
				DWORD flags;
				MCIERROR err;
				MCI_OPEN_PARMS mciOpenParms;

				if (playing != 0) {
					// STOP THIS SHIT
					MCI_GENERIC_PARMS mciGenericParams;
					mciGenericParams.dwCallback = 0; // (DWORD)_hwnd;
					DWORD flags = 0; // MCI_WAIT;
					MCIERROR err = mciSendCommand(mciMusicId, MCI_STOP, flags, (DWORD)(LPVOID)&mciGenericParams);
					if (err != 0) spitError("\n ::stopMusic:: ERROR STOPPING MUSIC!", err);
					playing = 0;
				}

				OutputDebugString("::: PLAYSONG THREADED!\n");

				if (mciMusicId != 0) {
					playing = 1;
					MCI_GENERIC_PARMS mciGenericParms;
					mciGenericParms.dwCallback = (DWORD)_hwnd;
					err = mciSendCommand(mciMusicId, MCI_CLOSE, 0, (DWORD)(LPVOID)&mciGenericParms);
					if (err != 0) spitError("\n::PLAYMUSIC:: ERROR CLOSING MUSIC!", err);
					//mciSendCommand(-1, MCI_STOP, 0, NULL);
					//mciSendCommand(-1, MCI_CLOSE, 0, NULL);
					mciMusicId = 0;

					mciSendCommand(mciMusicId, MCI_STOP, 0, NULL);
					mciSendCommand(mciMusicId, MCI_CLOSE, 0, NULL);
					playing = 0;

				}

				memset(&mciOpenParms, 0, sizeof(mciOpenParms));
				mciOpenParms.lpstrAlias = NULL;
				mciOpenParms.wDeviceID = 0;
				mciOpenParms.dwCallback = (DWORD)_hwnd;
				mciOpenParms.lpstrDeviceType = MCI_DEVTYPE_WAVEFORM_AUDIO;
				mciOpenParms.lpstrElementName = filename;

				// flags = MCI_OPEN_TYPE | MCI_WAIT | MCI_OPEN_ELEMENT;
				flags = MCI_OPEN_ELEMENT | MCI_WAIT;
				err = mciSendCommand(0, MCI_OPEN, flags, (DWORD)(LPVOID)&mciOpenParms);
				if (err != 0) {
					spitError("\n::PLAYMUSIC::ERROR OPENING MUSIC!", err);
					exit(err);
				} else {
					mciMusicId = mciOpenParms.wDeviceID;
					OutputDebugString("\n***** MUSIC OPENED. TRY TO PLAY IT. DeviceID:"); printout(mciMusicId);

					MCI_PLAY_PARMS mciPlayParams;
					mciPlayParams.dwCallback = (DWORD)_hwnd;
					mciPlayParams.dwFrom = 0;
					mciPlayParams.dwTo = 0;
					flags = MCI_NOTIFY;
					err = mciSendCommand(mciMusicId, MCI_PLAY, flags, (DWORD)&mciPlayParams);
					if (err != 0) {
						spitError("\n::PLAYMUSIC::ERROR PLAYING MUSIC!", err);
						exit(err);
					} else {
						OutputDebugString("\n***** MUSIC PLAYING SUCESSFULLY!\n");
						playing = 1;
					}
				}

				playSong = FALSE;
			} else {
				if (playSample == TRUE) {
					OutputDebugString("\n::THREAD:: PLAY SAMPLE: ");
					OutputDebugString(loopName); OutputDebugString("\n");

					//if (mciSampleId!=0) {
						// STOP THIS SHIT (at least outrun needs this)
						MCI_GENERIC_PARMS mciGenericParams;
						mciGenericParams.dwCallback = 0; // (DWORD)_hwnd;
						DWORD flags = MCI_WAIT;
						MCIERROR err = mciSendCommand(mciSampleId, MCI_STOP, flags, (DWORD)(LPVOID)&mciGenericParams);
						err = mciSendCommand(mciSampleId, MCI_CLOSE, flags, (DWORD)(LPVOID)&mciGenericParams);
						if (err != 0) spitError("\n ::stopSample:: ERROR STOPPING SAMPLE!", err);
						playing = 0;
					//}


					MCI_OPEN_PARMS mciOpenParms;
					memset(&mciOpenParms, 0, sizeof(mciOpenParms));
					mciOpenParms.lpstrAlias = NULL;
					mciOpenParms.wDeviceID = 0;
					mciOpenParms.dwCallback = 0; // (DWORD)_hwnd;
					mciOpenParms.lpstrDeviceType = MCI_DEVTYPE_WAVEFORM_AUDIO;
					mciOpenParms.lpstrElementName = loopName;

					// flags = MCI_OPEN_TYPE | MCI_WAIT | MCI_OPEN_ELEMENT;
					err = mciSendCommand(0, MCI_OPEN, MCI_OPEN_ELEMENT | MCI_WAIT, (DWORD)(LPVOID)&mciOpenParms);
					if (err != 0) {
						spitError("\n::PLAY SAMPLE::ERROR OPENING SAMPLE!", err);
						exit(err);
					} else {
						mciSampleId = mciOpenParms.wDeviceID;
						OutputDebugString("\n***** SAMPLE OPENED. TRY TO PLAY IT. DeviceID:"); printout(mciMusicId);

						MCI_PLAY_PARMS mciPlayParams;
						mciPlayParams.dwCallback = 0; // (DWORD)_hwnd;
						mciPlayParams.dwFrom = 0;
						mciPlayParams.dwTo = 0;
						err = mciSendCommand(mciSampleId, MCI_PLAY, MCI_NOTIFY, (DWORD)&mciPlayParams);
						if (err != 0) {
							spitError("\n::PLAYSAMPLE::ERROR PLAYING SAMPLE!", err);
							exit(err);
						}
						else {
							OutputDebugString("\n***** SAMPLE PLAYING SUCESSFULLY!\n");
						}
					}

					playSample = FALSE;
				}
			}
		}


		Sleep(1);
	}

	mciSendCommand(mciMusicId, MCI_STOP, 0, NULL);
	mciSendCommand(mciMusicId, MCI_CLOSE, 0, NULL);

	return 0;
}


int MSXPicoCreate() {
	msxPico = malloc(sizeof(MSXPico));
	
	destroyed = 0;
	mciMusicId = 0;

	kaboom = FALSE;
	playSong = FALSE;
	stopSong = FALSE;
	playSample = FALSE;
	pauseSong = FALSE;
	playSample = FALSE;

	//void ioPortRegister(int port, IoPortRead read, IoPortWrite write, void* ref)
	ioPortRegister(0x92, readIo, writeIo, msxPico);
	msxPico->ioState = 0;
	msxPico->loop = 0;
	loop = 0;
	lastMusic = -1;
	//strcpy(msxPico->filePath, "D:\\emulation\\MSX\\games\\pico\\The Goonies - Konami (1986) [MSX Pico WaveGame v1.1 Mixed]\\" );
	//strcpy(msxPico->fileName, filename);
	memset(filePath, 0, sizeof(filePath));

	OutputDebugString("*********** MSX PICO WAVE CREATED!\n");
	//OutputDebugString(filename);
	OutputDebugString("\n");

	CreateThread(NULL, 0, ThreadFunc, NULL, 0, NULL);
	return 1;
}


void MSXPicoDestroy() {
	destroyed = 1;

	sndPlaySound(NULL, SND_ASYNC);
	//mciSendCommand(MCI_ALL_DEVICE_ID, MCI_CLOSE, MCI_WAIT,NULL);

	mciSendCommand(mciMusicId, MCI_STOP, 0, NULL);
	mciSendCommand(mciMusicId, MCI_CLOSE, 0, NULL);


	ioPortUnregister(0x92);
	if (msxPico) free(msxPico); 
	msxPico = NULL;
	kaboom = TRUE;
}


static UInt8 readIo(MSXPico* msxPico, UInt16 ioPort) { return 0;  }

/*
00000000 : stop music
0000xxxx : fade out music in xxxx seconds
01xxxxxx : play song xxxxxx once (for instance command 01000101 will play 05.wav once)
10xxxxxx : play song xxxxxx in a loop (for instance command 10000101 will play 05.wav continuously)
11xxxxxx : toggle pause (lower 6 bits are ignored)
*/
static void writeIo(MSXPico* msxPico, UInt16 ioPort, UInt8 value) {
	char songname[100];
	int song = value & 63;

//	MCI_OPEN_PARMS mciOpenParms;
//	MCI_PLAY_PARMS mciPlayParams;

	OutputDebugString("***** WRITTEN ON MSXPICOWAVE PORT!\n");
	


	//HWAVEOUT hwo;
	//waveOutSetVolume(hwo, -1717986919);


	switch (value & 192) {
		case 192:
			// pause or resume
			OutputDebugString("Pause or resume song!\n");
			if (msxPico->ioState == 1) {
				sendMCI("pause MSXWAVE");
				msxPico->ioState = 2;

				char pauseFile[1024];
				strcpy(pauseFile, filePath);
				strcat(pauseFile, "\\pause.wav");
				sndPlaySound(pauseFile, SND_ASYNC);
			}
			else {
				sendMCI("resume MSXWAVE");
				msxPico->ioState = 1;
			}
			return;
			break;

		case 64:
			// play song once
			OutputDebugString("Play song once!\n");

			msxPico->ioState = 1;
			msxPico->loop = 0;
			loop = 0;
			//playing = 1;

			//sendMCI("stop MSXSND");
			//sendMCI("close MSXSND");
			
			//char loopName[2048];
			//sprintf(loopName, "open \"");
			//strcat(loopName, filePath);

			//memset(songname, 0, sizeof(songname));
			//sprintf(songname, "0%d.wav\" alias MSXSND", song);
			//strcat(loopName, songname);

			//sendMCI(loopName);
			//retval = sendMCI("setaudio MSXWAVE volume to 10");
			//sendMCI("play MSXSND");

			strcpy(loopName, filePath);

			memset(songname, 0, sizeof(songname));
			if (song < 10) {
				sprintf(songname, "0%d.wav", song);
			} else {
				sprintf(songname, "%d.wav", song);
			}
			strcat(loopName, songname);
			//sndPlaySound(TEXT(loopName), SND_NODEFAULT | SND_ASYNC);
			playSample = TRUE;
			stopSong = FALSE;
			playSong = FALSE;

			break;

		case 128:
			//if (msxPico->ioState == 1 || playing == 1) {
				//sendMCI("stop MSXWAVE");
				//sendMCI("close MSXWAVE");
			//}

			OutputDebugString("Play song looped!\n");
			if (msxPico) {
				msxPico->ioState = 1;
				msxPico->loop = 1;
			}
			loop = 1;
			//sndPlaySound(filename, SND_ASYNC |SND_LOOP);

			/*
			memset(filename, 0, sizeof(filename));
			//sprintf(filename, "D:\\emulation\\MSX\\games\\pico\\The Goonies - Konami (1986) [MSX Pico WaveGame v1.1 Mixed]\\0%d.wav\0", song);
			sprintf(filename, "open \"");
			strcat(filename, filePath);

			sprintf(songname, "0%d.wav\" alias MSXWAVE", song);
			strcat(filename, songname);
			OutputDebugString(filename);
			sendMCI(filename);
			//retval = sendMCI("setaudio MSXWAVE volume to 10");
			sendMCI("play MSXWAVE from 1 notify");
			/**/

			/**/
			lastMusic = song;
			memset(filename, 0, sizeof(filename));
			strcpy(filename, filePath);
			if (song < 10) {
				sprintf(songname, "0%d.wav", song);
			}
			else {
				sprintf(songname, "%d.wav", song);
			}
			strcat(filename, songname);

			//playMusic();
			stopSong = FALSE;
			playSong = TRUE;
			/**/
			return;
			break;

		case 0:
			// stop song
			OutputDebugString("::write port:: Stop song\n");
			//sndPlaySound("", SND_NODEFAULT);
			//sendMCI("stop MSXWAV wait");

			//stopMusic();
			playSong = FALSE;
			playSample = FALSE;
			stopSong = TRUE;
			playing = 1;
			loop = 0;
			//playing = 0;
			msxPico->loop = 0;
			msxPico->ioState = 0;

			break;
	}
}

static void stopMusic() {
	/*

	if (mciMusicId != 0 && playing!=0) {
		playing = 0;
		OutputDebugString("\n **** ::stopMusic:: TRYING TO STOP MUSIC mciId: "); printout(mciMusicId);
		MCI_GENERIC_PARMS mciGenericParams;
		mciGenericParams.dwCallback = 0; // (DWORD)_hwnd;
		DWORD flags = 0; // MCI_WAIT;
		MCIERROR err = mciSendCommand(mciMusicId, MCI_STOP, flags, (DWORD)(LPVOID)&mciGenericParams);
		if (err != 0) {
			spitError("\n ::stopMusic:: ERROR STOPPING MUSIC!", err);
		} else {
			playing = 0;
			mciMusicId = 0;
		}
	}
*/
}

static void spitError(char *text, MCIERROR mcie) {
	OutputDebugString(text);  printout(mcie);
	TCHAR buffer[256];
	if (mciGetErrorString(mcie, &buffer[0], sizeof(buffer))) OutputDebugString(buffer);
}

static void playMusic() {
	/*
	DWORD flags;
	MCIERROR err;
	MCI_OPEN_PARMS mciOpenParms;

	if (mciMusicId != 0) {
		MCI_GENERIC_PARMS mciGenericParms;
		mciGenericParms.dwCallback = (DWORD)_hwnd;
		err = mciSendCommand(mciMusicId, MCI_CLOSE, 0, (DWORD)(LPVOID)&mciGenericParms);
		if (err != 0) spitError("\n::PLAYMUSIC:: ERROR CLOSING MUSIC!", err);
		//mciSendCommand(-1, MCI_STOP, 0, NULL);
		//mciSendCommand(-1, MCI_CLOSE, 0, NULL);
		mciMusicId = 0;

		mciSendCommand(mciMusicId, MCI_STOP, 0, NULL);
		mciSendCommand(mciMusicId, MCI_CLOSE, 0, NULL);

	}

	memset(&mciOpenParms, 0, sizeof(mciOpenParms));
	mciOpenParms.lpstrAlias = NULL;
	mciOpenParms.wDeviceID = 0;
	mciOpenParms.dwCallback = (DWORD) _hwnd;
	mciOpenParms.lpstrDeviceType = MCI_DEVTYPE_WAVEFORM_AUDIO;
	mciOpenParms.lpstrElementName = filename;
	
	// flags = MCI_OPEN_TYPE | MCI_WAIT | MCI_OPEN_ELEMENT;
	flags = MCI_OPEN_ELEMENT | MCI_WAIT;
	err = mciSendCommand(0, MCI_OPEN, flags, (DWORD)(LPVOID)&mciOpenParms);
	if (err != 0) {
		spitError("\n::PLAYMUSIC::ERROR OPENING MUSIC!", err);
		exit(err);
	} else {
		mciMusicId = mciOpenParms.wDeviceID;
		OutputDebugString("\n***** MUSIC OPENED. TRY TO PLAY IT. DeviceID:"); printout(mciMusicId);

		MCI_PLAY_PARMS mciPlayParams;
		mciPlayParams.dwCallback = (DWORD)_hwnd;
		mciPlayParams.dwFrom = 0;
		mciPlayParams.dwTo = 0;
		flags = MCI_NOTIFY;
		err = mciSendCommand(mciMusicId, MCI_PLAY, flags, (DWORD)&mciPlayParams);
		if (err != 0) {
			spitError("\n::PLAYMUSIC::ERROR PLAYING MUSIC!", err);
			exit(err);
		} else {
			OutputDebugString("\n***** MUSIC PLAYING SUCESSFULLY!\n");
		}
	}
	*/
}

/*
MCI Error Strings MCI Error Numbers
MCIERR_BASE 256
MCIERR_INVALID_DEVICE_ID 257
MCIERR_UNRECOGNIZED_KEYWORD 259
MCIERR_UNRECOGNIZED_COMMAND 261
MCIERR_HARDWARE 262
MCIERR_INVALID_DEVICE_NAME 263
MCIERR_OUT_OF_MEMORY 264
MCIERR_DEVICE_OPEN 265
MCIERR_CANNOT_LOAD_DRIVER 266
MCIERR_MISSING_COMMAND_STRING 267
MCIERR_PARAM_OVERFLOW 268
MCIERR_MISSING_STRING_ARGUMENT 269
MCIERR_BAD_INTEGER 270
MCIERR_PARSER_INTERNAL 271
MCIERR_DRIVER_INTERNAL 272
MCIERR_MISSING_PARAMETER 273
MCIERR_UNSUPPORTED_FUNCTION 274
MCIERR_FILE_NOT_FOUND 275
MCIERR_DEVICE_NOT_READY 276
MCIERR_INTERNAL 277
MCIERR_DRIVER 278
MCIERR_CANNOT_USE_ALL 279
MCIERR_MULTIPLE 280
MCIERR_EXTENSION_NOT_FOUND 281
MCIERR_OUTOFRANGE 282
MCIERR_FLAGS_NOT_COMPATIBLE 283
MCIERR_FILE_NOT_SAVED 286
MCIERR_DEVICE_TYPE_REQUIRED 287
MCIERR_DEVICE_LOCKED 288
MCIERR_DUPLICATE_ALIAS 289
MCIERR_BAD_CONSTANT 290
MCIERR_MUST_USE_SHAREABLE 291
MCIERR_MISSING_DEVICE_NAME 292
MCIERR_BAD_TIME_FORMAT 293
MCIERR_NO_CLOSING_QUOTE 294
MCIERR_DUPLICATE_FLAGS 295
MCIERR_INVALID_FILE 296
MCIERR_NULL_PARAMETER_BLOCK 297
MCIERR_UNNAMED_RESOURCE 298
MCIERR_NEW_REQUIRES_ALIAS 299
MCIERR_NOTIFY_ON_AUTO_OPEN 300
MCIERR_NO_ELEMENT_ALLOWED 301
MCIERR_NONAPPLICABLE_FUNCTION 302
MCIERR_ILLEGAL_FOR_AUTO_OPEN 303
MCIERR_FILENAME_REQUIRED 304
MCIERR_EXTRA_CHARACTERS 305
MCIERR_DEVICE_NOT_INSTALLED 306
MCIERR_GET_CD 307
MCIERR_SET_CD 308
MCIERR_SET_DRIVE 309
MCIERR_DEVICE_LENGTH 310
MCIERR_DEVICE_ORD_LENGTH 311
MCIERR_NO_INTEGER 312
MCIERR_WAVE_OUTPUTSINUSE 320
MCIERR_WAVE_SETOUTPUTINUSE 321
MCIERR_WAVE_INPUTSINUSE 322
MCIERR_WAVE_SETINPUTINUSE 323
MCIERR_WAVE_OUTPUTUNSPECIFIED 324
MCIERR_WAVE_INPUTUNSPECIFIED 325
MCIERR_WAVE_OUTPUTSUNSUITABLE 326
MCIERR_WAVE_SETOUTPUTUNSUITABLE 327
MCIERR_WAVE_INPUTSUNSUITABLE 328
MCIERR_WAVE_SETINPUTUNSUITABLE 329
MCIERR_SEQ_DIV_INCOMPATIBLE 336
MCIERR_SEQ_PORT_INUSE 337
MCIERR_SEQ_PORT_NONEXISTENT 338
MCIERR_SEQ_PORT_MAPNODEVICE 339
MCIERR_SEQ_PORT_MISCERROR 340
MCIERR_SEQ_TIMER 341
MCIERR_SEQ_PORTUNSPECIFIED 342
MCIERR_SEQ_NOMIDIPRESENT 343
MCIERR_NO_WINDOW 346
MCIERR_CREATEWINDOW 347
MCIERR_FILE_READ 348
MCIERR_FILE_WRITE 349
MCIERR_CUSTOM_DRIVER_BASE 512
*/
static MCIERROR sendMCI(char *command) {
	//_hwnd = getMainHwnd();
	MCIERROR me = mciSendString(TEXT(command), NULL, 0, _hwnd);
	if (me != 0) {
		OutputDebugString("*****> MCI command error: ");
		printout(me);
		OutputDebugString("\n\n");
		OutputDebugString("*****> command: ");
		OutputDebugString(command);
		OutputDebugString("\n\n");
		TCHAR buffer[128];
		if (mciGetErrorString(me, &buffer[0], sizeof(buffer))) OutputDebugString(buffer);

	}
	return me;
}


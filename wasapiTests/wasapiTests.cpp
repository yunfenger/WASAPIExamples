#define _USE_MATH_DEFINES

#include <tchar.h>
#include <iostream>
#include <math.h>
#include <string>

#include <Windows.h>
#include <audiopolicy.h>
#include <Audioclient.h>
#include <mmdeviceapi.h>

using namespace std;

#define REFTIMES_PER_SEC 10000000
#define REFTIMES_PER_MILLISEC 10000

#define EXIT_ON_ERROR(hres) \
	if (FAILED(hres)) { \
		cout << "ERROR: " << hex << "0x" << hres << endl << \
		__FILE__ << ": " << \
		oct << __LINE__ << endl; goto Exit;}

#define SAFE_RELEASE(punk) \
	if ((punk) != NULL) \
		{ (punk)->Release(); (punk) = NULL;}

//////////////////////////////////////////////////////////////////////////////

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

typedef struct MyAudioSource {
	float sineFrequency_ = 880.0;
	double pos_ = 0.0;
	double cycleLength_ = 44100.0 / sineFrequency_;
	
	WAVEFORMATEX* pwfx_ = NULL;
	
	void setFormat(WAVEFORMATEX* pwfx) {
		pwfx_ = pwfx;
		cycleLength_ = pwfx_->nSamplesPerSec / sineFrequency_;
	}

	void loadData(UINT32 numFramesAvailable, BYTE* pData, DWORD* flags) {
		float* out = (float*)pData;

		for (int frame = 0; frame < numFramesAvailable * pwfx_->nChannels; ++frame) {
			float val = sin(2 * M_PI * (pos_ / cycleLength_));
			
			for (int chan = 0; chan < pwfx_->nChannels; ++chan) {
				out[frame] = val;
			}
			
			pos_ += 1.0;
			if (pos_ > cycleLength_)
				pos_ -= cycleLength_;
		}

//		for (int i = 0; i < numFramesAvailable; ++i)
//			cout << out[i] << " ";
	}

} MyAudioSource;

HRESULT PlayAudioStream(MyAudioSource* pMySource)
{
	HRESULT hr;
	REFERENCE_TIME hnsRequestDuration = REFTIMES_PER_SEC;
	REFERENCE_TIME hnsActualDuration;
	IMMDeviceEnumerator* pEnumerator = NULL;
	IMMDevice* pDevice = NULL;
	IAudioClient* pAudioClient = NULL;
	IAudioRenderClient* pRenderClient = NULL;
	WAVEFORMATEX* pwfx = NULL;
	UINT32 bufferFrameCount;
	UINT32 numFramesAvailable;
	UINT32 numFramesPadding;
	BYTE* pData = nullptr;
	DWORD flags = 0;

	EXIT_ON_ERROR(CoInitializeEx(NULL, COINIT_SPEED_OVER_MEMORY));

	EXIT_ON_ERROR(CoCreateInstance(
			CLSID_MMDeviceEnumerator,
			NULL,
			CLSCTX_ALL,
			IID_IMMDeviceEnumerator,
			(void**)&pEnumerator));

	EXIT_ON_ERROR(pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice));

	EXIT_ON_ERROR(pDevice->Activate(IID_IAudioClient,
		CLSCTX_ALL,
		NULL,
		(void**)&pAudioClient));

	EXIT_ON_ERROR(pAudioClient->GetMixFormat(&pwfx));

	EXIT_ON_ERROR(pAudioClient->Initialize(
		AUDCLNT_SHAREMODE_SHARED,
		0,
		hnsRequestDuration,
		0,
		pwfx,
		NULL));

	// format to use
	pMySource->setFormat(pwfx);

	EXIT_ON_ERROR(pAudioClient->GetBufferSize(&bufferFrameCount));

	EXIT_ON_ERROR(pAudioClient->GetService(IID_IAudioRenderClient, (void**)&pRenderClient));

	EXIT_ON_ERROR(pRenderClient->GetBuffer(bufferFrameCount, &pData));

	pMySource->loadData(bufferFrameCount, pData, &flags);

	EXIT_ON_ERROR(pRenderClient->ReleaseBuffer(bufferFrameCount, flags));

	// Calculate duration
	hnsActualDuration = (double)REFTIMES_PER_SEC * bufferFrameCount / pwfx->nSamplesPerSec;

	EXIT_ON_ERROR(pAudioClient->Start());

	// Loop to fill *ABOUT* half lol OMG
	while (flags != AUDCLNT_BUFFERFLAGS_SILENT)
	{
		// Sleep for half buffer duration -.-
		Sleep((DWORD)(hnsActualDuration / REFTIMES_PER_MILLISEC / 2));

		// See how much space is available.
		EXIT_ON_ERROR(pAudioClient->GetCurrentPadding(&numFramesPadding));

		numFramesAvailable = bufferFrameCount - numFramesPadding;

		// Grab available space in shared buffer.
		EXIT_ON_ERROR(pRenderClient->GetBuffer(numFramesAvailable, &pData));

		// Get next 1/2 second of data.
		pMySource->loadData(numFramesAvailable, pData, &flags);

		EXIT_ON_ERROR(pRenderClient->ReleaseBuffer(numFramesAvailable, flags));
	}

	// Wait for last data in buffer to play before stopping.
	Sleep((DWORD)(hnsActualDuration / REFTIMES_PER_MILLISEC / 2));

	EXIT_ON_ERROR(pAudioClient->Stop());

Exit:
	CoTaskMemFree(pwfx);
	SAFE_RELEASE(pEnumerator)
	SAFE_RELEASE(pDevice)
	SAFE_RELEASE(pAudioClient)
	SAFE_RELEASE(pRenderClient)

	return 1;
}



///////////////////////////////////////////////////////////////////////////////////////////



int _tmain(int argc, _TCHAR* argv[])
{
	MyAudioSource pMySource;
	PlayAudioStream(&pMySource);


	cout << "Press enter to exit." << endl;
	getchar();
	return 0;
}


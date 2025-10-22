// Copyright iraj mohtasham aurelion.net 2023


#include "EncoderThread.h"

#include "CaptureSubsystem.h"


FEncoderThread::FEncoderThread()
{

	VideoDataQueue=nullptr;
	AudioDataQueue=nullptr;
}

FEncoderThread::~FEncoderThread()
{
	// Ensure delegates are unbound to avoid cross-thread access while destroying
	VideoEncodeDelegate.Unbind();
	AudioEncodeDelegate.Unbind();
	ThreadInitDelegate.Unbind();

	VideoDataQueue.Reset();
	AudioDataQueue.Reset();
}

bool FEncoderThread::Init()
{
	UE_LOG(LogCaptureSubsystem, Log, TEXT("FEncoderThread::Init called on thread"));
	if (ThreadInitDelegate.IsBound())
	{
		UE_LOG(LogCaptureSubsystem, Log, TEXT("FEncoderThread::Init executing ThreadInitDelegate"));
		ThreadInitDelegate.Execute();
	}
	return true;
}

uint32 FEncoderThread::Run()
{
	while ((!bStopped) || (!IsFinished()))
	{
		RunEncode();
	}
	return 1;
}

void FEncoderThread::Stop()
{
	bStopped = true;
}

void FEncoderThread::Exit()
{
}

void FEncoderThread::CreateVideoQueue()
{
	UE_LOG(LogCaptureSubsystem,Log,TEXT("Creating Video Queue"))
	VideoDataQueue=MakeUnique<TCircularQueue<FVideoData>>(60 );
}

void FEncoderThread::CreateAudioQueue()
{
	UE_LOG(LogCaptureSubsystem,Log,TEXT("Creating Audio Queue"))
	AudioDataQueue=MakeUnique<TCircularQueue<FAudioData>>(60 );

}

bool FEncoderThread::IsAudioThreadInitialized() const
{
	if (AudioDataQueue)
	{
		return true;
	}
	return false;
}


void FEncoderThread::InsertVideo(void* TextureData, float DeltaTime)
{
	if (bStopped)
	{
		return ;
	}
	if (!VideoDataQueue)
	{
		return ;
	}


		FScopeLock ScopeLock(&VideoBufferMutex);
		VideoDataQueue->Enqueue(FVideoData(DeltaTime,TextureData));


}

bool FEncoderThread::InsertAudio(void* Data, float AudioClock) const
{
	if (bStopped)
	{
		return false;
	}
	if (!AudioDataQueue)
	{
		return false;
	}
	AudioDataQueue->Enqueue(FAudioData(AudioClock,Data));

	return true;
}



void FEncoderThread::RunEncode()
{
	{
		FScopeLock ScopeLock(&AudioMutex);
		EncodeAudio();
	}

	{
		FScopeLock ScopeLock1(&VideoBufferMutex);
		{
			EncodeVideo();
		}
	}
}

void FEncoderThread::EncodeVideo() const
{
	FVideoData Data;

	if(VideoDataQueue->Dequeue(Data))
	{
		VideoEncodeDelegate.ExecuteIfBound(Data);
		// 消费端语义：编码线程负责释放由渲染线程分配的纹理缓冲
		if (Data.TextureData)
		{
			FMemory::Free(Data.TextureData);
			Data.TextureData = nullptr;
		}
	}

}

void FEncoderThread::EncodeAudio() const
{
	if (!AudioDataQueue)
	{
		return;
	}

	FAudioData Data;
	if (AudioDataQueue->Dequeue(Data))
	{
		// 调用消费回调
		AudioEncodeDelegate.ExecuteIfBound(Data);

		// 释放复制的音频缓冲（生产端在 OnNewSubmixBuffer 中分配）
		if (Data.Data)
		{
			FMemory::Free(Data.Data);
			Data.Data = nullptr;
		}
	}
}

bool FEncoderThread::IsFinished() const
{
	if(!VideoDataQueue||!AudioDataQueue)
	{
		return false;
	}
	return VideoDataQueue->IsEmpty()&&AudioDataQueue->IsEmpty();

}

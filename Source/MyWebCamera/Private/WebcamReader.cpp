// Fill out your copyright notice in the Description page of Project Settings.


#include "WebcamReader.h"

using namespace cv;
using namespace std;

// Sets default values
AWebcamReader::AWebcamReader()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

    // Initialize OpenCV and webcam properties
    CameraID = 0;
    OperationMode = 0;
    RefreshRate = 15;
    isStreamOpen = false;
    VideoSize = FVector2D(0, 0);
    ShouldResize = false;
    ResizeDeminsions = FVector2D(320, 240);
    RefreshTimer = 0.0f;
    // 03 �п���2 192.168.10.248
    stream = cv::VideoCapture();
    frame = cv::Mat();
}

// Called when the game starts or when spawned
void AWebcamReader::BeginPlay()
{
	Super::BeginPlay();
	
    // Open the stream
    bool b = stream.open("rtsp://admin:admin13579@192.168.10.248/h264/ch1/main/av_stream");  // CameraID
    if (stream.isOpened())
    {
        // Initialize stream
        isStreamOpen = true;
        UpdateFrame();
        VideoSize = FVector2D(frame.cols, frame.rows);
        size = cv::Size(ResizeDeminsions.X, ResizeDeminsions.Y);
        VideoTexture = UTexture2D::CreateTransient(VideoSize.X, VideoSize.Y);
        VideoTexture->UpdateResource();
        VideoUpdateTextureRegion = new FUpdateTextureRegion2D(0, 0, 0, 0, VideoSize.X, VideoSize.Y);

        // Initialize data array
        Data.Init(FColor(0, 0, 0, 255), VideoSize.X * VideoSize.Y);

        // Do first frame
        DoProcessing();
        UpdateTexture();
        OnNextVideoFrame();
    }
}

// Called every frame
void AWebcamReader::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

    RefreshTimer += DeltaTime;
    if (isStreamOpen && RefreshTimer >= 1.0f / RefreshRate)
    {
        RefreshTimer -= 1.0f / RefreshRate;
        UpdateFrame();
        DoProcessing();
        UpdateTexture();
        OnNextVideoFrame();
    }
}

//TCHAR arrayתCHAR array��
//
//const char* msg = TCHAR_TO_ANSI(TEXT("dddd"));
//
//
//CHAR arrayתTCHAR array��
//
//const TCHAR* msg = ANSI_TO_TCHAR("dddd");


void AWebcamReader::ChangeOperation()
{
    OperationMode++;
    OperationMode %= 3;
}

void AWebcamReader::ChangeFilename(FString Filename)
{
    const TCHAR* p = *Filename;
    
    String s = TCHAR_TO_ANSI(*Filename);

    // Initialize stream
    stream.open(s);

    isStreamOpen = stream.isOpened();
}

void AWebcamReader::UpdateFrame()
{
    if (stream.isOpened())
    {
        stream.read(frame);
        if (ShouldResize)
        {
            cv::resize(frame, frame, size);
        }
    }
    else {
        isStreamOpen = false;
    }
}

void AWebcamReader::DoProcessing()
{
    // TODO: Do any processing with frame here!

    if (OperationMode == 0) {
        // Apply nothing
    }
    else if (OperationMode == 1) {
        // Apply median blur
        cv::Mat src = frame.clone();
        cv::medianBlur(src, frame, 7);
    }
    else if (OperationMode == 2) {
        cv::Mat src, dst;
        cv::cvtColor(frame, src, cv::COLOR_RGB2GRAY);

        int thresh = 50;

        Canny(src, dst, thresh, thresh * 2, 3);
        cv::cvtColor(dst, frame, cv::COLOR_GRAY2BGR);
    }

}

void AWebcamReader::UpdateTexture()
{
    if (isStreamOpen && frame.data)
    {
        // Copy Mat data to Data array
        for (int y = 0; y < VideoSize.Y; y++)
        {
            for (int x = 0; x < VideoSize.X; x++)
            {
                int i = x + (y * VideoSize.X);
                Data[i].B = frame.data[i * 3 + 0];
                Data[i].G = frame.data[i * 3 + 1];
                Data[i].R = frame.data[i * 3 + 2];
            }
        }

        // Update texture 2D
        UpdateTextureRegions(VideoTexture, (int32)0, (uint32)1, VideoUpdateTextureRegion, (uint32)(4 * VideoSize.X), (uint32)4, (uint8*)Data.GetData(), false);
    }
}

void AWebcamReader::UpdateTextureRegions(UTexture2D* Texture, int32 MipIndex, uint32 NumRegions, FUpdateTextureRegion2D* Regions, uint32 SrcPitch, uint32 SrcBpp, uint8* SrcData, bool bFreeData)
{
    if (Texture->Resource)
    {
        struct FUpdateTextureRegionsData
        {
            FTexture2DResource* Texture2DResource;
            int32 MipIndex;
            uint32 NumRegions;
            FUpdateTextureRegion2D* Regions;
            uint32 SrcPitch;
            uint32 SrcBpp;
            uint8* SrcData;
        };

        FUpdateTextureRegionsData* RegionData = new FUpdateTextureRegionsData;

        RegionData->Texture2DResource = (FTexture2DResource*)Texture->Resource;
        RegionData->MipIndex = MipIndex;
        RegionData->NumRegions = NumRegions;
        RegionData->Regions = Regions;
        RegionData->SrcPitch = SrcPitch;
        RegionData->SrcBpp = SrcBpp;
        RegionData->SrcData = SrcData;

        ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
            UpdateTextureRegionsData,
            FUpdateTextureRegionsData*, RegionData, RegionData,
            bool, bFreeData, bFreeData,
            {
                for (uint32 RegionIndex = 0; RegionIndex < RegionData->NumRegions; ++RegionIndex)
                {
                    int32 CurrentFirstMip = RegionData->Texture2DResource->GetCurrentFirstMip();
                    if (RegionData->MipIndex >= CurrentFirstMip)
                    {
                        RHIUpdateTexture2D(
                            RegionData->Texture2DResource->GetTexture2DRHI(),
                            RegionData->MipIndex - CurrentFirstMip,
                            RegionData->Regions[RegionIndex],
                            RegionData->SrcPitch,
                            RegionData->SrcData
                            + RegionData->Regions[RegionIndex].SrcY * RegionData->SrcPitch
                            + RegionData->Regions[RegionIndex].SrcX * RegionData->SrcBpp
                        );
                    }
                }
        if (bFreeData)
        {
            FMemory::Free(RegionData->Regions);
            FMemory::Free(RegionData->SrcData);
        }
        delete RegionData;
            });
    }
}

#include "SplashGame.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/ConfigCacheIni.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "UnrealClient.h"

FString ASplashRange::QualityLabel() const {
    const TCHAR* Names[]={TEXT("PERFORMANCE"),TEXT("BALANCED"),TEXT("HIGH")};
    return Names[FMath::Clamp(GraphicsPreset,0,2)];
}
void ASplashRange::ApplyQuality() {
    auto Set=[](const TCHAR* Name,int Value) { if(auto* V=IConsoleManager::Get().FindConsoleVariable(Name)) V->Set(Value,ECVF_SetByCode); };
    Set(TEXT("r.ScreenPercentage"),GraphicsPreset==0?83:100);
    Set(TEXT("r.Shadow.MaxResolution"),GraphicsPreset==0?1024:(GraphicsPreset==1?2048:4096));
    Set(TEXT("r.Shadow.CSM.MaxCascades"),GraphicsPreset==0?2:3);
    Set(TEXT("r.SSR.Quality"),GraphicsPreset+1);
    Set(TEXT("r.AmbientOcclusionLevels"),GraphicsPreset==2?2:1);
    Set(TEXT("r.MaxAnisotropy"),GraphicsPreset==0?4:8);
    Set(TEXT("r.VolumetricFog"),0);
    Set(TEXT("r.SSGI.Quality"),GraphicsPreset==2?3:0);
    if(Camera) {
        auto& PP=Camera->GetCameraComponent()->PostProcessSettings;
        PP.bOverride_DynamicGlobalIlluminationMethod=true;
        PP.DynamicGlobalIlluminationMethod=GraphicsPreset==2?EDynamicGlobalIlluminationMethod::ScreenSpace:EDynamicGlobalIlluminationMethod::None;
    }
    Set(TEXT("t.MaxFPS"),60);
    UE_LOG(LogTemp,Display,TEXT("CLANKA_GRAPHICS %s"),*QualityLabel());
}
void ASplashRange::ToggleQuality() {
    GraphicsPreset=(GraphicsPreset+1)%3; ApplyQuality();
    const FString Filename=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("ClankaSettings.ini"));
    GConfig->SetInt(TEXT("Clanka"),TEXT("GraphicsPreset"),GraphicsPreset,Filename); GConfig->Flush(false,Filename);
    Notice=TEXT("GRAPHICS  /  ")+QualityLabel(); NoticeSeconds=3;
}
void ASplashRange::SaveBenchmark() {
    if(FrameMilliseconds.IsEmpty()) return;
    TArray<double> Sorted=FrameMilliseconds; Sorted.Sort();
    double Total=0; int OverBudget=0; for(double Ms:Sorted) { Total+=Ms; if(Ms>33.3333) ++OverBudget; }
    auto Percentile=[&](double P){ return Sorted[FMath::Clamp(int((Sorted.Num()-1)*P),0,Sorted.Num()-1)]; };
    FString PerCamera;
    for(int Cam=0;Cam<3;++Cam) {
        int Count=0; double Sum=0; TArray<double> Times;
        for(int I=0;I<FrameMilliseconds.Num();++I) if(FrameCameras[I]==Cam) { Sum+=FrameMilliseconds[I]; ++Count; Times.Add(FrameMilliseconds[I]); }
        Times.Sort(); const double P95=Count?Times[int((Count-1)*.95)]:0;
        PerCamera+=FString::Printf(TEXT("%s{\"camera\":%d,\"frames\":%d,\"average_fps\":%.2f,\"p95_ms\":%.2f}"),Cam?TEXT(","):TEXT(""),Cam,Count,Sum>0?Count*1000.0/Sum:0,P95);
    }
    int Width=0,Height=0; GetWorld()->GetFirstPlayerController()->GetViewportSize(Width,Height);
    const FString Result=FString::Printf(TEXT("{\"preset\":\"%s\",\"width\":%d,\"height\":%d,\"screen_percentage\":%d,\"frames\":%d,\"measured_seconds\":%.3f,\"average_fps\":%.2f,\"p95_ms\":%.2f,\"p99_ms\":%.2f,\"worst_ms\":%.2f,\"frames_over_33_33ms\":%d,\"popped\":%d,\"cameras\":[%s],\"method\":\"Real frame intervals after 10s warmup; screenshot frames excluded; 60fps cap; all 3 cameras\"}"),*QualityLabel(),Width,Height,GraphicsPreset==0?83:100,Sorted.Num(),Total/1000.0,Sorted.Num()*1000.0/Total,Percentile(.95),Percentile(.99),Sorted.Last(),OverBudget,Sim.popped,*PerCamera);
    FFileHelper::SaveStringToFile(Result,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Performance.json")));
    FString CSV=TEXT("camera,frame_ms\n"); for(int I=0;I<FrameMilliseconds.Num();++I) CSV+=FString::Printf(TEXT("%d,%.4f\n"),FrameCameras[I],FrameMilliseconds[I]);
    FFileHelper::SaveStringToFile(CSV,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Performance.csv")));
    UE_LOG(LogTemp,Display,TEXT("CLANKA_PERFORMANCE %s"),*Result);
}
void ASplashRange::UpdateDiagnostics(float Dt) {
    const double Now=FPlatformTime::Seconds(), Elapsed=Now-PerfStart, FrameMs=(Now-LastFrameClock)*1000; LastFrameClock=Now;
    if(FrameMs>0) SmoothedFPS=FMath::FInterpTo(SmoothedFPS,float(1000/FrameMs),FMath::Min(Dt,.1f),2);
    if(bMenuCapture) {
        if(Elapsed>5 && !bScreenshot) { bScreenshot=true; FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Menu.png")),false,false); }
        if(Elapsed>8) FGenericPlatformMisc::RequestExit(false);
    }
    if(bBenchmark) {
        // Allow rendering/shaders to settle, then sample three camera angles.
        if(Elapsed>=10 && Elapsed<BenchmarkDuration && !(Elapsed>14.8 && Elapsed<17) && !(Elapsed>34.8 && Elapsed<37) && !(Elapsed>54.8 && Elapsed<57)) { FrameMilliseconds.Add(FrameMs); FrameCameras.Add(CameraIndex); }
        if(Elapsed>25 && BenchmarkStage==0) { CameraIndex=1; BenchmarkStage=1; }
        if(Elapsed>45 && BenchmarkStage==1) { CameraIndex=2; BenchmarkStage=2; }
        const int Shot=Elapsed>55?3:(Elapsed>35?2:(Elapsed>15?1:0));
        if(Shot>TestStage) { TestStage=Shot; FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),FString::Printf(TEXT("Realism-%d.png"),Shot)),false,false); }
        if(Elapsed>=BenchmarkDuration) { SaveBenchmark(); FGenericPlatformMisc::RequestExit(false); }
    }
    if(bInteractionTest) {
        auto* HUD=Cast<ASplashHUD>(GetWorld()->GetFirstPlayerController()->GetHUD()); if(!HUD) return;
        auto Check=[&](bool Condition,const TCHAR* Label){if(!Condition) InteractionFailures.Add(Label);};
        if(Elapsed>1 && TestStage==0) { Check(!Sim.running,TEXT("Initial power off")); HUD->NotifyHitBoxClick(TEXT("on")); ++TestStage; }
        else if(Elapsed>2 && TestStage==1) { Check(Sim.running&&!Sim.verified&&Sim.popped==0,TEXT("ON tracks without water")); HUD->NotifyHitBoxClick(TEXT("engage")); ++TestStage; }
        else if(Elapsed>8 && TestStage==2) { Check(Sim.popped>0,TEXT("ENGAGE automatically pops")); HUD->NotifyHitBoxClick(TEXT("pause")); PausedTestTime=Sim.time; ++TestStage; }
        else if(Elapsed>9 && TestStage==3) { Check(Sim.paused&&Sim.time==PausedTestTime,TEXT("Pause freezes time")); HUD->NotifyHitBoxClick(TEXT("pause")); HUD->NotifyHitBoxClick(TEXT("stop")); ++TestStage; }
        else if(Elapsed>10 && TestStage==4) { Check(Sim.stopped&&!Sim.verified&&!Sim.spraying,TEXT("Stop disarms")); HUD->NotifyHitBoxClick(TEXT("stop")); ++TestStage; }
        else if(Elapsed>11 && TestStage==5) { Check(!Sim.stopped&&!Sim.verified,TEXT("Release still requires ENGAGE")); HUD->NotifyHitBoxClick(TEXT("engage")); HUD->NotifyHitBoxClick(TEXT("help")); HUD->NotifyHitBoxClick(TEXT("close_help")); ++TestStage; }
        else if(Elapsed>12 && TestStage==6) { Check(!bHelp&&Sim.verified,TEXT("Help closes and engagement resumes")); bPhotoMode=true; HUD->NotifyHitBoxClick(TEXT("off")); ++TestStage; }
        else if(Elapsed>13 && TestStage==7) { Check(!Sim.running&&!Sim.verified,TEXT("Power off disarms")); Check(!bPhotoMode,TEXT("Power off restores menu visibility")); bPhotoMode=true; HUD->NotifyHitBoxClick(TEXT("reset")); ++TestStage; }
        else if(Elapsed>14 && TestStage==8) {
            Check(!Sim.running&&Sim.popped==0,TEXT("Reset returns to menu")); Check(!bPhotoMode,TEXT("Reset restores menu visibility"));
            FString Result=FString::Printf(TEXT("{\"passed\":%s,\"checks\":11,\"failures\":\"%s\",\"method\":\"Calls actual HUD button handlers; does not inject mouse clicks\"}"),InteractionFailures.IsEmpty()?TEXT("true"):TEXT("false"),*FString::Join(InteractionFailures,TEXT("; ")));
            FFileHelper::SaveStringToFile(Result,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("InteractionTest.json"))); ++TestStage; FGenericPlatformMisc::RequestExit(false);
        }
    }
}

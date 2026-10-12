#include "SplashGame.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/AudioComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureCube.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include "InputCoreTypes.h"

static FVector WorldPoint(splash::Vec P) { return FVector(P.x,P.y,P.z); }
static FLinearColor BalloonColor(int I,splash::BalloonKind Kind) {
    if(Kind==splash::BalloonKind::Bonus) return FLinearColor(1,.55f,.035f);
    if(Kind==splash::BalloonKind::Tough) return FLinearColor(.52f,.2f,1);
    const FLinearColor Colors[]={FLinearColor(1,.11f,.24f),FLinearColor(.02f,.56f,1),FLinearColor(.04f,.9f,.55f),FLinearColor(1,.25f,.055f),FLinearColor(.8f,.075f,.5f)};
    return Colors[I%5];
}
ASplashRange::ASplashRange() { PrimaryActorTick.bCanEverTick=true; RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root")); }
UStaticMeshComponent* ASplashRange::Mesh(const TCHAR* Shape,FVector Position,FVector Size,FLinearColor Color) {
    auto* C=NewObject<UStaticMeshComponent>(this); C->SetupAttachment(RootComponent); C->SetMobility(EComponentMobility::Movable);
    C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"),Shape,Shape)));
    C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->RegisterComponent(); C->SetWorldLocation(Position); C->SetWorldScale3D(Size);
    if(Material) { auto* M=UMaterialInstanceDynamic::Create(Material,this); M->SetVectorParameterValue(TEXT("Tint"),Color); M->SetScalarParameterValue(TEXT("Roughness"),.72f); C->SetMaterial(0,M); }
    return C;
}
UStaticMeshComponent* ASplashRange::Glow(const TCHAR* Shape,FVector Position,FVector Size,FLinearColor Color,float Brightness) {
    auto* C=Mesh(Shape,Position,Size,Color); C->SetCastShadow(false);
    if(GlowMaterial) { auto* M=UMaterialInstanceDynamic::Create(GlowMaterial,this); M->SetVectorParameterValue(TEXT("Tint"),Color*Brightness); C->SetMaterial(0,M); }
    return C;
}
void ASplashRange::BeginPlay() {
    Super::BeginPlay();
    Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_Splash.M_Splash"));
    GlowMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_Glow.M_Glow"));
    BuildArena();
    auto* Latex=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Realism/M_Latex.M_Latex"));
    for(int I=0;I<static_cast<int>(Sim.balloons.size());++I) {
        auto* B=Mesh(TEXT("Sphere"),WorldPoint(Sim.balloons[I].p),FVector(1.35f,1.35f,1.65f),BalloonColor(I,Sim.balloons[I].kind));
        if(Latex) { auto* M=UMaterialInstanceDynamic::Create(Latex,this); M->SetVectorParameterValue(TEXT("Tint"),BalloonColor(I,Sim.balloons[I].kind)); B->SetMaterial(0,M); }
        BalloonMeshes.Add(B); BalloonMaterials.Add(Cast<UMaterialInstanceDynamic>(B->GetMaterial(0)));
        if(BalloonMaterials.Last()) BalloonMaterials.Last()->SetScalarParameterValue(TEXT("Roughness"),.27f);
        BalloonKnots.Add(Mesh(TEXT("Cone"),WorldPoint(Sim.balloons[I].p)-FVector(0,0,85),FVector(.15f,.15f,.22f),BalloonColor(I,Sim.balloons[I].kind)));
        Ribbons.Add(Mesh(TEXT("Cylinder"),WorldPoint(Sim.balloons[I].p)-FVector(0,0,130),FVector(.018f,.018f,1.1f),FLinearColor(.65f,.77f,.8f)));
    }
    auto MakeInstances=[this](FLinearColor Color,const TCHAR* Shape) {
        auto* C=NewObject<UInstancedStaticMeshComponent>(this); C->SetupAttachment(RootComponent); C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"),Shape,Shape))); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetCastShadow(false); C->RegisterComponent();
        auto* FXMat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Realism/M_Droplet.M_Droplet"));
        if(!FXMat) FXMat=Material;
        if(FXMat) { auto* M=UMaterialInstanceDynamic::Create(FXMat,this); M->SetVectorParameterValue(TEXT("Tint"),Color); M->SetScalarParameterValue(TEXT("Roughness"),.08f); C->SetMaterial(0,M); } return C;
    };
    Water=MakeInstances(FLinearColor(.65f,.87f,.95f),TEXT("Sphere"));
    Confetti.Add(MakeInstances(FLinearColor(.7f,.85f,.91f),TEXT("Sphere")));
    Confetti.Add(MakeInstances(FLinearColor(.72f,.85f,.92f),TEXT("Sphere")));
    Confetti.Add(MakeInstances(FLinearColor(.72f,.85f,.92f),TEXT("Sphere")));
    auto* Sun=GetWorld()->SpawnActor<ADirectionalLight>();
    auto* Light=Cast<UDirectionalLightComponent>(Sun->GetLightComponent());
    Light->SetMobility(EComponentMobility::Movable); Sun->SetActorRotation(FRotator(-38,40,0));
    Light->SetIntensity(5.5f); Light->SetLightColor(FLinearColor(1,.92f,.82f));
    Light->bAtmosphereSunLight=true; Light->DynamicShadowDistanceMovableLight=18000;
    Light->DynamicShadowCascades=3; Light->LightSourceAngle=1.1f; Light->ShadowBias=.35f;
    Light->ContactShadowLength=.12f;
    auto* SkyActor=GetWorld()->SpawnActor<AActor>();
    auto* Atmosphere=NewObject<USkyAtmosphereComponent>(SkyActor); SkyActor->SetRootComponent(Atmosphere); Atmosphere->RegisterComponent();
    auto* Sky=GetWorld()->SpawnActor<ASkyLight>(); auto* SkyLight=Sky->GetLightComponent();
    SkyLight->SetMobility(EComponentMobility::Movable); SkyLight->SetIntensity(1.5f);
    if(auto* Cube=LoadObject<UTextureCube>(nullptr,TEXT("/Game/Realism/T_Environment.T_Environment"))) {
        SkyLight->SourceType=SLS_SpecifiedCubemap; SkyLight->SetCubemap(Cube); SkyLight->SetSourceCubemapAngle(75);
    }
    SkyLight->SetLowerHemisphereColor(FLinearColor(.16f,.17f,.18f)); SkyLight->RecaptureSky();
    if(auto* SkyMat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Realism/M_Sky.M_Sky"))) {
        auto* Dome=Mesh(TEXT("Sphere"),FVector::ZeroVector,FVector(2000),FLinearColor::White);
        Dome->SetMaterial(0,SkyMat); Dome->SetCastShadow(false);
    }
    auto* Fog=GetWorld()->SpawnActor<AExponentialHeightFog>();
    Fog->GetComponent()->SetFogDensity(.0015f); Fog->GetComponent()->SetFogHeightFalloff(.12f);
    Fog->GetComponent()->SetFogInscatteringColor(FLinearColor(.39f,.37f,.31f));
    Fog->GetComponent()->SetStartDistance(6500); Fog->GetComponent()->SetFogMaxOpacity(.25f);
    Fog->GetComponent()->SetVolumetricFog(false);
    Camera=GetWorld()->SpawnActor<ACameraActor>(); Camera->GetCameraComponent()->SetFieldOfView(65); Camera->GetCameraComponent()->bConstrainAspectRatio=false;
    auto& PP=Camera->GetCameraComponent()->PostProcessSettings;
    PP.bOverride_AutoExposureMethod=true; PP.AutoExposureMethod=EAutoExposureMethod::AEM_Manual;
    PP.bOverride_AutoExposureBias=true; PP.AutoExposureBias=-.65f; PP.bOverride_AutoExposureApplyPhysicalCameraExposure=true; PP.AutoExposureApplyPhysicalCameraExposure=false;
    PP.bOverride_BloomIntensity=true; PP.BloomIntensity=.18f; PP.bOverride_VignetteIntensity=true; PP.VignetteIntensity=.22f;
    PP.bOverride_AmbientOcclusionIntensity=true; PP.AmbientOcclusionIntensity=1.05f;
    PP.bOverride_AmbientOcclusionRadius=true; PP.AmbientOcclusionRadius=110;
    PP.bOverride_ScreenSpaceReflectionIntensity=true; PP.ScreenSpaceReflectionIntensity=85;
    PP.bOverride_ScreenSpaceReflectionQuality=true; PP.ScreenSpaceReflectionQuality=50;
    PP.bOverride_ColorContrast=true; PP.ColorContrast=FVector4(1.04f,1.04f,1.04f,1);
    PP.bOverride_ColorSaturation=true; PP.ColorSaturation=FVector4(.94f,.94f,.94f,1);
    Camera->GetCameraComponent()->PostProcessBlendWeight=1;
    CameraIndex=0; UpdateCamera(100);
    auto* PC=GetWorld()->GetFirstPlayerController(); PC->bShowMouseCursor=true; PC->bEnableClickEvents=true; PC->SetViewTarget(Camera); PC->SetInputMode(FInputModeGameAndUI());
    PopSound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Audio/S_Pop.S_Pop")); ConfirmSound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Audio/S_Confirm.S_Confirm")); WaveSound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Audio/S_Wave.S_Wave"));
    WaterAudio=NewObject<UAudioComponent>(this); WaterAudio->SetupAttachment(RootComponent); WaterAudio->bAutoActivate=false; WaterAudio->SetSound(LoadObject<USoundBase>(nullptr,TEXT("/Game/Audio/S_WaterLoop.S_WaterLoop"))); WaterAudio->RegisterComponent();
    Ambience=NewObject<UAudioComponent>(this); Ambience->SetupAttachment(RootComponent); Ambience->bAutoActivate=false; Ambience->SetSound(LoadObject<USoundBase>(nullptr,TEXT("/Game/Audio/S_Ambience.S_Ambience"))); Ambience->RegisterComponent(); Ambience->SetVolumeMultiplier(.2f); Ambience->Play();
    FString SavedScore; if(FFileHelper::LoadFileToString(SavedScore,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("BestScore.txt")))) BestScore=FCString::Atoi(*SavedScore);
    bDemoTest=FParse::Param(FCommandLine::Get(),TEXT("DemoTest"));
    bBenchmark=FParse::Param(FCommandLine::Get(),TEXT("PerfTest"));
    bMenuCapture=FParse::Param(FCommandLine::Get(),TEXT("MenuCapture"));
    bInteractionTest=FParse::Param(FCommandLine::Get(),TEXT("InteractionTest"));
    FParse::Value(FCommandLine::Get(),TEXT("PerfSeconds="),BenchmarkDuration);
    BenchmarkDuration=FMath::Clamp(BenchmarkDuration,60.0,180.0);
    FParse::Value(FCommandLine::Get(),TEXT("Camera="),CameraIndex);
    GConfig->GetInt(TEXT("Clanka"),TEXT("GraphicsPreset"),GraphicsPreset,FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("ClankaSettings.ini")));
    FParse::Value(FCommandLine::Get(),TEXT("Quality="),GraphicsPreset);
    GraphicsPreset=FMath::Clamp(GraphicsPreset,0,2); CameraIndex=FMath::Clamp(CameraIndex,0,2);
    bPhotoMode=FParse::Param(FCommandLine::Get(),TEXT("PhotoMode"));
    ApplyQuality(); PerfStart=LastFrameClock=FPlatformTime::Seconds();
    if(bDemoTest || bBenchmark) { StartGame(true); Command(splash::Action::Engage); }
}
void ASplashRange::StartGame(bool Autopilot) {
    Sim.action(splash::Action::PowerOn); Sim.automatic=true; bAutopilot=Autopilot; bHelp=false;
    if(bSound && ConfirmSound) UGameplayStatics::PlaySound2D(this,ConfirmSound,.45f);
    Notice=TEXT("SYSTEM ONLINE  //  Press ENGAGE to begin automatic spraying"); NoticeSeconds=4;
}
void ASplashRange::ReturnToMenu() { SaveBest(); Sim.action(splash::Action::PowerOff); bHelp=true; bPhotoMode=false; }
void ASplashRange::Command(splash::Action A) {
    if(A==splash::Action::Reset) { bPhotoMode=false; SaveBest(); LastPopSerial=0; LastPopped=0; LastWave=1; Particles.Empty(); FloatingScores.Empty(); }
    Sim.action(A); if(bSound && ConfirmSound) UGameplayStatics::PlaySound2D(this,ConfirmSound,.32f);
}
void ASplashRange::Select(int Index) {
    if(!Sim.running || Sim.paused || Sim.stopped) return;
    if(Index>=0 && Index<static_cast<int>(Sim.balloons.size()) && Sim.eligible(Sim.balloons[Index])) { Sim.selected=Index; Notice=FString::Printf(TEXT("Prioritizing track B-%02d"),Index+1); NoticeSeconds=2; }
}
void ASplashRange::ReadClick() {
    auto* PC=GetWorld()->GetFirstPlayerController(); if(!PC || !Sim.running || bHelp || bPhotoMode) return;
    float MX=0,MY=0; if(!PC->GetMousePosition(MX,MY)) return; int X=0,Y=0; PC->GetViewportSize(X,Y);
    if(PC->GetHUD() && PC->GetHUD()->GetHitBoxAtCoordinates(FVector2D(MX,MY),true)) return;
    if(MY<100.f*Y/900 || MY>750.f*Y/900) return;
    float Best=70.f; int Index=-1;
    for(int I=0;I<static_cast<int>(Sim.balloons.size());++I) { if(!Sim.eligible(Sim.balloons[I])) continue; FVector2D P; if(PC->ProjectWorldLocationToScreen(WorldPoint(Sim.balloons[I].p),P)) { float D=FVector2D::Distance(P,FVector2D(MX,MY)); if(D<Best) { Best=D; Index=I; } } }
    if(Index>=0) Select(Index);
}
void ASplashRange::ChangeCamera() { CameraIndex=(CameraIndex+1)%3; }
void ASplashRange::UpdateCamera(float Dt) {
    const FVector Positions[]={FVector(-2000,-1650,850),FVector(-1450,-220,500),FVector(950,-2100,2040)};
    FVector Focus(900,100,450), Position=Focus+(Positions[CameraIndex]-Focus)*CameraZoom;
    if(!Sim.running) Position+=FVector(FMath::Sin(SceneTime*.08f)*90,FMath::Cos(SceneTime*.08f)*70,0);
    Camera->SetActorLocation(FMath::VInterpTo(Camera->GetActorLocation(),Position,Dt,3)); Camera->SetActorRotation((Focus-Camera->GetActorLocation()).Rotation());
}
void ASplashRange::ToggleSound() { bSound=!bSound; if(Ambience) Ambience->SetVolumeMultiplier(bSound?.2f:0); }
void ASplashRange::SaveBest() { if(Sim.score>BestScore) { BestScore=Sim.score; IFileManager::Get().MakeDirectory(*FPaths::ProjectSavedDir(),true); FFileHelper::SaveStringToFile(FString::FromInt(BestScore),*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("BestScore.txt"))); } }
void ASplashRange::ExportLog() {
    FString CSV=TEXT("seconds,event\n"); for(const auto& E:Sim.events) CSV+=FString::Printf(TEXT("%.3f,\"%s\"\n"),E.time,UTF8_TO_TCHAR(E.message.c_str()));
    CSV+=FString::Printf(TEXT("%.3f,\"SUMMARY popped=%d exited=%d water=%.1f\"\n"),Sim.time,Sim.popped,Sim.escaped,Sim.tank);
    IFileManager::Get().MakeDirectory(*FPaths::ProjectSavedDir(),true); const bool OK=FFileHelper::SaveStringToFile(CSV,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("SessionLog.csv")));
    Notice=OK?TEXT("Session log saved"):TEXT("Session log could not be saved"); NoticeSeconds=3;
}
void ASplashRange::EndPlay(const EEndPlayReason::Type Reason) { SaveBest(); ExportLog(); Super::EndPlay(Reason); }
void ASplashRange::UpdateEffects(float Dt) {
    for(const auto& Pop:Sim.popEvents) if(Pop.serial>LastPopSerial) {
        LastPopSerial=Pop.serial; const FVector P=WorldPoint(Pop.p); int Kind=Pop.kind==splash::BalloonKind::Bonus?1:(Pop.kind==splash::BalloonKind::Tough?2:0);
        for(int I=0;I<30;++I) { FVector Velocity=FXRandom.VRand()*FXRandom.FRandRange(100,410)+FVector(0,0,150); Particles.Add({P,Velocity,0,float(FXRandom.FRandRange(.65f,1.35f)),float(FXRandom.FRandRange(.035f,.11f)),Kind}); }
        FloatingScores.Add({P,1,0});
        if(bSound && PopSound) UGameplayStatics::PlaySound2D(this,PopSound,.5f,FXRandom.FRandRange(.9f,1.2f));
    }
    for(auto& P:Particles) { P.Age+=Dt; P.Position+=P.Velocity*Dt; P.Velocity.Z-=Dt*420; }
    Particles.RemoveAll([](const FWaterParticle& P){return P.Age>=P.Life;});
    for(auto& C:Confetti) C->ClearInstances();
    for(const auto& P:Particles) { float S=P.Size*(1-P.Age/P.Life); Confetti[P.Kind]->AddInstance(FTransform(FRotator(P.Age*160,P.Age*250,0),P.Position,FVector(S)),true); }
    for(auto& F:FloatingScores) { F.Age+=Dt; F.Position.Z+=Dt*80; } FloatingScores.RemoveAll([](const FScoreFloat& F){return F.Age>1.15f;});
    Water->ClearInstances(); for(const auto& D:Sim.drops) Water->AddInstance(FTransform(WorldPoint(D.v).Rotation(),WorldPoint(D.p),FVector(.39f,.065f,.065f)),true);
    if(Sim.paused || !bSound) { WaterAudioLevel=0; WaterAudio->Stop(); return; }
    float TargetVolume=Sim.spraying && bSound && !Sim.paused?.17f:0;
    WaterAudioLevel=FMath::FInterpTo(WaterAudioLevel,TargetVolume,Dt,10); WaterAudio->SetVolumeMultiplier(WaterAudioLevel);
    if(TargetVolume>0 && !WaterAudio->IsPlaying()) WaterAudio->Play();
    if(TargetVolume==0 && WaterAudioLevel<.002f) WaterAudio->Stop();
}
void ASplashRange::Tick(float Dt) {
    Super::Tick(Dt); SceneTime+=Dt; NoticeSeconds=FMath::Max(0.f,NoticeSeconds-Dt); WaveBanner=FMath::Max(0.f,WaveBanner-Dt);
    auto* PC=GetWorld()->GetFirstPlayerController(); if(PC) {
        using A=splash::Action;
        if(PC->WasInputKeyJustPressed(EKeys::Enter)) { if(!Sim.running) StartGame(true); else Command(A::Engage); }
        if(PC->WasInputKeyJustPressed(EKeys::G)) Command(A::Engage);
        if(PC->WasInputKeyJustPressed(EKeys::SpaceBar) || PC->WasInputKeyJustPressed(EKeys::P)) Command(A::Pause);
        if(PC->WasInputKeyJustPressed(EKeys::E)) Command(A::Stop);
        if(PC->WasInputKeyJustPressed(EKeys::T)) Command(A::ReleaseStop);
        if(PC->WasInputKeyJustPressed(EKeys::V) && Sim.running) Command(A::Verify);
        if(PC->WasInputKeyJustPressed(EKeys::F)) Command(A::Refill);
        if(PC->WasInputKeyJustPressed(EKeys::R)) Command(A::Reset);
        if(PC->WasInputKeyJustPressed(EKeys::One)) Command(A::Calm);
        if(PC->WasInputKeyJustPressed(EKeys::Two)) Command(A::Breeze);
        if(PC->WasInputKeyJustPressed(EKeys::Three)) Command(A::Gusts);
        if(PC->WasInputKeyJustPressed(EKeys::C)) ChangeCamera();
        if(PC->WasInputKeyJustPressed(EKeys::Q)) ToggleQuality();
        if(PC->WasInputKeyJustPressed(EKeys::Tab)) bPhotoMode=!bPhotoMode;
        if(PC->WasInputKeyJustPressed(EKeys::M)) ToggleSound();
        if(PC->WasInputKeyJustPressed(EKeys::H)) bHelp=!bHelp;
        if(PC->WasInputKeyJustPressed(EKeys::L)) ExportLog();
        if(PC->WasInputKeyJustPressed(EKeys::Escape)) { if(Sim.running) ReturnToMenu(); else FGenericPlatformMisc::RequestExit(false); }
        if(PC->WasInputKeyJustPressed(EKeys::LeftMouseButton)) ReadClick();
        if(PC->WasInputKeyJustPressed(EKeys::MouseScrollUp)) CameraZoom=FMath::Max(.75f,CameraZoom-.06f);
        if(PC->WasInputKeyJustPressed(EKeys::MouseScrollDown)) CameraZoom=FMath::Min(1.3f,CameraZoom+.06f);
    }
    Accumulator+=FMath::Min(Dt,.1f); while(Accumulator>=splash::Simulation::Step) { Sim.tick(); Accumulator-=splash::Simulation::Step; }
    for(int I=0;I<BalloonMeshes.Num();++I) {
        const auto& B=Sim.balloons[I]; auto* MeshComponent=BalloonMeshes[I].Get(); FVector Position=WorldPoint(B.p);
        if(!Sim.running) Position.Z+=FMath::Sin(SceneTime*1.2f+I)*20;
        MeshComponent->SetVisibility(B.alive); MeshComponent->SetWorldLocation(Position);
        float Size=B.kind==splash::BalloonKind::Tough?1.55f:1.3f; MeshComponent->SetWorldScale3D(FVector(Size,Size,Size*1.25f)*(1+B.wet*.1));
        BalloonKnots[I]->SetVisibility(B.alive); BalloonKnots[I]->SetWorldLocation(Position-FVector(0,0,Size*62+7));
        Ribbons[I]->SetVisibility(B.alive); Ribbons[I]->SetWorldLocation(Position-FVector(0,0,Size*50+55)); Ribbons[I]->SetWorldRotation(FRotator(FMath::Sin(SceneTime*2+I)*8,0,0));
        if(BalloonMaterials[I]) BalloonMaterials[I]->SetVectorParameterValue(TEXT("Tint"),B.protectedZone?FLinearColor(.22f,.3f,.34f):FMath::Lerp(BalloonColor(I,B.kind),FLinearColor(.25f,.8f,1),float(B.wet*.4)));
    }
    FVector Direction=WorldPoint(Sim.aim); Head->SetWorldRotation(Direction.Rotation()); FRotator BarrelRotation=FRotationMatrix::MakeFromZ(Direction).Rotator(); Barrel->SetWorldRotation(BarrelRotation); Barrel->SetWorldLocation(WorldPoint(Sim.nozzle)+Direction*120); Muzzle->SetWorldRotation(BarrelRotation); Muzzle->SetWorldLocation(WorldPoint(Sim.nozzle)+Direction*222);
    UpdateEffects(Sim.paused?0:Dt); UpdateCamera(Dt); UpdateDiagnostics(Dt);
    if(Sim.wave!=LastWave) { LastWave=Sim.wave; WaveBanner=3; if(bSound && WaveSound) UGameplayStatics::PlaySound2D(this,WaveSound,.3f); }
    if(bDemoTest) {
        DemoWallTime+=Dt;
        if(DemoWallTime>15 && !bScreenshot) { bScreenshot=true; FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("UnrealDemo.png")),false,false); }
        if(DemoWallTime>20) { FString Result=FString::Printf(TEXT("{\"popped\":%d,\"water\":%.2f,\"seconds\":%.2f,\"unreal\":true}"),Sim.popped,Sim.tank,Sim.time); FFileHelper::SaveStringToFile(Result,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("UnrealDemoResult.json"))); FGenericPlatformMisc::RequestExit(false); }
    }
}
ASplashGameMode::ASplashGameMode() { HUDClass=ASplashHUD::StaticClass(); DefaultPawnClass=nullptr; }
void ASplashGameMode::BeginPlay() { Super::BeginPlay(); GetWorld()->SpawnActor<ASplashRange>(); }

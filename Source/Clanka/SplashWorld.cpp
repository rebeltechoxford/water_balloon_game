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
void ASplashRange::BuildArena() {
    // Floating water laboratory: raised deck, luminous lanes, botanical perimeter.
    Mesh(TEXT("Cube"),FVector(950,0,-105),FVector(52,38,1.8f),FLinearColor(.028f,.08f,.13f));
    Mesh(TEXT("Cube"),FVector(950,0,-12),FVector(50,36,.14f),FLinearColor(.085f,.22f,.29f));
    Glow(TEXT("Cube"),FVector(950,-1850,-50),FVector(51,.07f,.12f),FLinearColor(.02f,.7f,1),2);
    Glow(TEXT("Cube"),FVector(950,1850,-50),FVector(51,.07f,.12f),FLinearColor(.02f,.7f,1),2);
    for(int I=-8;I<=19;++I) Mesh(TEXT("Cube"),FVector(I*175,0,-3),FVector(.015f,35,.016f),FLinearColor(.15f,.34f,.4f));
    for(int I=-10;I<=10;++I) Mesh(TEXT("Cube"),FVector(950,I*175,-3),FVector(50,.015f,.016f),FLinearColor(.15f,.34f,.4f));
    Mesh(TEXT("Cube"),FVector(1500,940,-1),FVector(30,6.4f,.035f),FLinearColor(.22f,.15f,.09f));
    for(int I=0;I<16;++I) { auto* Stripe=Glow(TEXT("Cube"),FVector(I*190,628,2),FVector(.85f,.055f,.025f),FLinearColor(1,.43f,.055f)); Stripe->SetWorldRotation(FRotator(0,-25,0)); }
    // Range gate and beacons.
    for(int Side:{-1,1}) {
        Mesh(TEXT("Cube"),FVector(2750,Side*1400,700),FVector(1.1f,1.1f,14),FLinearColor(.065f,.15f,.23f));
        Glow(TEXT("Cube"),FVector(2689,Side*1400,700),FVector(.035f,.25f,12.8f),FLinearColor(.05f,.8f,1),2);
        for(int I=0;I<8;++I) {
            float X=-950+I*560; Mesh(TEXT("Cylinder"),FVector(X,Side*1700,72),FVector(.42f,.42f,1.44f),FLinearColor(.16f,.27f,.32f));
            Glow(TEXT("Sphere"),FVector(X,Side*1700,155),FVector(.25f),FLinearColor(.04f,.82f,1),1.7f);
        }
    }
    Mesh(TEXT("Cube"),FVector(2750,0,1440),FVector(1.1f,29,1),FLinearColor(.055f,.12f,.2f));
    Glow(TEXT("Cube"),FVector(2690,0,1430),FVector(.03f,26,.06f),FLinearColor(.14f,.8f,1),2);
    auto* Sign=NewObject<UTextRenderComponent>(this); Sign->SetupAttachment(RootComponent); Sign->RegisterComponent(); Sign->SetWorldLocation(FVector(2685,0,1510)); Sign->SetWorldRotation(FRotator(0,180,0)); Sign->SetHorizontalAlignment(EHTA_Center); Sign->SetWorldSize(105); Sign->SetText(FText::FromString(TEXT("S P L A S H   L A B"))); Sign->SetTextRenderColor(FColor(155,233,255));
    // Stylized landscape instead of an empty template level.
    for(int I=0;I<20;++I) {
        float Angle=I*PI*2/20, Radius=5000+FXRandom.FRandRange(0,1600);
        FVector P(900+FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,-160);
        float Tall=FXRandom.FRandRange(11,24);
        Mesh(TEXT("Cone"),P+FVector(0,0,Tall*40),FVector(30,30,Tall),FLinearColor(.06f,.16f+I%3*.03f,.21f+I%4*.03f));
        if(I%2==0) { FVector Cloud=P*.8f+FVector(0,0,2200); for(int J=0;J<3;++J) Mesh(TEXT("Sphere"),Cloud+FVector(J*250,0,J%2*80),FVector(7,4,2.2f),FLinearColor(.35f,.58f,.68f)); }
    }
    for(int I=0;I<9;++I) {
        FVector P(-1200+I*540,2200+(I%2)*180,0);
        Mesh(TEXT("Cylinder"),P+FVector(0,0,160),FVector(.6f,.6f,3.2f),FLinearColor(.16f,.12f,.08f));
        Mesh(TEXT("Cone"),P+FVector(0,0,460),FVector(4.5f,4.5f,6),FLinearColor(.035f,.3f,.21f));
        Mesh(TEXT("Cone"),P+FVector(0,0,680),FVector(3,3,4.5f),FLinearColor(.055f,.4f,.27f));
    }
    // Water tank and cannon station.
    Mesh(TEXT("Cylinder"),FVector(-440,330,140),FVector(2.8f,2.8f,2.8f),FLinearColor(.04f,.27f,.38f));
    for(int I=0;I<3;++I) Glow(TEXT("Cylinder"),FVector(-440,330,30+I*110),FVector(2.84f,2.84f,.045f),FLinearColor(.07f,.66f,.94f),1.5f);
    Mesh(TEXT("Cube"),FVector(-240,190,45),FVector(3.5f,.3f,.3f),FLinearColor(.04f,.3f,.4f))->SetWorldRotation(FRotator(0,-35,0));
    Mesh(TEXT("Cylinder"),FVector(0,0,25),FVector(3.6f,3.6f,.5f),FLinearColor(.04f,.1f,.17f));
    Glow(TEXT("Cylinder"),FVector(0,0,52),FVector(3.3f,3.3f,.04f),FLinearColor(.035f,.75f,1),2);
    Mesh(TEXT("Cylinder"),FVector(0,0,90),FVector(1.6f,1.6f,.7f),FLinearColor(.25f,.43f,.5f));
    Head=Mesh(TEXT("Sphere"),WorldPoint(Sim.nozzle),FVector(1.7f,1.55f,1.4f),FLinearColor(.7f,.83f,.82f));
    Barrel=Mesh(TEXT("Cylinder"),WorldPoint(Sim.nozzle),FVector(.52f,.52f,2),FLinearColor(.08f,.35f,.46f));
    Muzzle=Glow(TEXT("Cylinder"),WorldPoint(Sim.nozzle),FVector(.62f,.62f,.14f),FLinearColor(.1f,.8f,1),2);
}
void ASplashRange::BeginPlay() {
    Super::BeginPlay();
    Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_Splash.M_Splash"));
    GlowMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_Glow.M_Glow"));
    BuildArena();
    for(int I=0;I<static_cast<int>(Sim.balloons.size());++I) {
        auto* B=Mesh(TEXT("Sphere"),WorldPoint(Sim.balloons[I].p),FVector(1.35f,1.35f,1.65f),BalloonColor(I,Sim.balloons[I].kind));
        BalloonMeshes.Add(B); BalloonMaterials.Add(Cast<UMaterialInstanceDynamic>(B->GetMaterial(0)));
        if(BalloonMaterials.Last()) BalloonMaterials.Last()->SetScalarParameterValue(TEXT("Roughness"),.27f);
        Ribbons.Add(Mesh(TEXT("Cylinder"),WorldPoint(Sim.balloons[I].p)-FVector(0,0,130),FVector(.018f,.018f,1.1f),FLinearColor(.65f,.77f,.8f)));
    }
    auto MakeInstances=[this](FLinearColor Color,const TCHAR* Shape) {
        auto* C=NewObject<UInstancedStaticMeshComponent>(this); C->SetupAttachment(RootComponent); C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"),Shape,Shape))); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetCastShadow(false); C->RegisterComponent();
        if(GlowMaterial) { auto* M=UMaterialInstanceDynamic::Create(GlowMaterial,this); M->SetVectorParameterValue(TEXT("Tint"),Color); C->SetMaterial(0,M); } return C;
    };
    Water=MakeInstances(FLinearColor(.07f,.62f,1)*2,TEXT("Sphere"));
    Confetti.Add(MakeInstances(FLinearColor(.2f,1,.75f)*1.5f,TEXT("Sphere")));
    Confetti.Add(MakeInstances(FLinearColor(1,.6f,.09f)*1.5f,TEXT("Cube")));
    Confetti.Add(MakeInstances(FLinearColor(.6f,.25f,1)*1.5f,TEXT("Cube")));
    auto* Sun=GetWorld()->SpawnActor<ADirectionalLight>(); auto* Light=Cast<UDirectionalLightComponent>(Sun->GetLightComponent()); Light->SetMobility(EComponentMobility::Movable); Sun->SetActorRotation(FRotator(-32,-45,0)); Light->SetIntensity(4.5f); Light->SetLightColor(FLinearColor(1,.82f,.65f)); Light->bAtmosphereSunLight=true; Light->DynamicShadowDistanceMovableLight=12000; Light->DynamicShadowCascades=2;
    auto* SkyActor=GetWorld()->SpawnActor<AActor>(); auto* Atmosphere=NewObject<USkyAtmosphereComponent>(SkyActor); SkyActor->SetRootComponent(Atmosphere); Atmosphere->RegisterComponent();
    auto* Sky=GetWorld()->SpawnActor<ASkyLight>(); Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable); Sky->GetLightComponent()->SetIntensity(.65f); Sky->GetLightComponent()->RecaptureSky();
    auto* Fog=GetWorld()->SpawnActor<AExponentialHeightFog>(); Fog->GetComponent()->SetFogDensity(.008f); Fog->GetComponent()->SetFogInscatteringColor(FLinearColor(.08f,.22f,.3f)); Fog->GetComponent()->SetStartDistance(3200);
    Camera=GetWorld()->SpawnActor<ACameraActor>(); Camera->GetCameraComponent()->SetFieldOfView(58); Camera->GetCameraComponent()->bConstrainAspectRatio=false;
    auto& PP=Camera->GetCameraComponent()->PostProcessSettings;
    PP.bOverride_AutoExposureMethod=true; PP.AutoExposureMethod=EAutoExposureMethod::AEM_Manual;
    PP.bOverride_AutoExposureBias=true; PP.AutoExposureBias=1.2f; PP.bOverride_AutoExposureApplyPhysicalCameraExposure=true; PP.AutoExposureApplyPhysicalCameraExposure=false;
    PP.bOverride_BloomIntensity=true; PP.BloomIntensity=.35f; PP.bOverride_VignetteIntensity=true; PP.VignetteIntensity=.3f;
    Camera->GetCameraComponent()->PostProcessBlendWeight=1;
    CameraIndex=0; UpdateCamera(100);
    auto* PC=GetWorld()->GetFirstPlayerController(); PC->bShowMouseCursor=true; PC->bEnableClickEvents=true; PC->SetViewTarget(Camera); PC->SetInputMode(FInputModeGameAndUI());
    PopSound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Audio/S_Pop.S_Pop")); ConfirmSound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Audio/S_Confirm.S_Confirm")); WaveSound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Audio/S_Wave.S_Wave"));
    WaterAudio=NewObject<UAudioComponent>(this); WaterAudio->SetupAttachment(RootComponent); WaterAudio->bAutoActivate=false; WaterAudio->SetSound(LoadObject<USoundBase>(nullptr,TEXT("/Game/Audio/S_WaterLoop.S_WaterLoop"))); WaterAudio->RegisterComponent();
    Ambience=NewObject<UAudioComponent>(this); Ambience->SetupAttachment(RootComponent); Ambience->bAutoActivate=false; Ambience->SetSound(LoadObject<USoundBase>(nullptr,TEXT("/Game/Audio/S_Ambience.S_Ambience"))); Ambience->RegisterComponent(); Ambience->SetVolumeMultiplier(.2f); Ambience->Play();
    FString SavedScore; if(FFileHelper::LoadFileToString(SavedScore,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("BestScore.txt")))) BestScore=FCString::Atoi(*SavedScore);
    bDemoTest=FParse::Param(FCommandLine::Get(),TEXT("DemoTest"));
    if(bDemoTest) { StartGame(true); Command(splash::Action::Engage); }
}
void ASplashRange::StartGame(bool Autopilot) {
    Sim.action(splash::Action::PowerOn); Sim.automatic=true; bAutopilot=Autopilot; bHelp=false;
    if(bSound && ConfirmSound) UGameplayStatics::PlaySound2D(this,ConfirmSound,.45f);
    Notice=TEXT("SYSTEM ONLINE  //  Press ENGAGE to begin automatic spraying"); NoticeSeconds=4;
}
void ASplashRange::ReturnToMenu() { SaveBest(); Sim.action(splash::Action::PowerOff); bHelp=true; }
void ASplashRange::Command(splash::Action A) {
    if(A==splash::Action::Reset) { SaveBest(); LastPopSerial=0; LastPopped=0; LastWave=1; Particles.Empty(); FloatingScores.Empty(); }
    Sim.action(A); if(bSound && ConfirmSound) UGameplayStatics::PlaySound2D(this,ConfirmSound,.32f);
}
void ASplashRange::Select(int Index) {
    if(!Sim.running || Sim.paused || Sim.stopped) return;
    if(Index>=0 && Index<static_cast<int>(Sim.balloons.size()) && Sim.eligible(Sim.balloons[Index])) { Sim.selected=Index; Notice=FString::Printf(TEXT("Prioritizing track B-%02d"),Index+1); NoticeSeconds=2; }
}
void ASplashRange::ReadClick() {
    auto* PC=GetWorld()->GetFirstPlayerController(); if(!PC || !Sim.running) return;
    float MX=0,MY=0; if(!PC->GetMousePosition(MX,MY)) return; int X=0,Y=0; PC->GetViewportSize(X,Y);
    if(MY<100.f*Y/900 || MY>750.f*Y/900) return;
    float Best=70.f; int Index=-1;
    for(int I=0;I<static_cast<int>(Sim.balloons.size());++I) { if(!Sim.eligible(Sim.balloons[I])) continue; FVector2D P; if(PC->ProjectWorldLocationToScreen(WorldPoint(Sim.balloons[I].p),P)) { float D=FVector2D::Distance(P,FVector2D(MX,MY)); if(D<Best) { Best=D; Index=I; } } }
    if(Index>=0) Select(Index);
}
void ASplashRange::ChangeCamera() { CameraIndex=(CameraIndex+1)%3; }
void ASplashRange::UpdateCamera(float Dt) {
    const FVector Positions[]={FVector(-2100,-2900,1950),FVector(-1400,-50,620),FVector(900,-2200,2550)};
    FVector Focus(1080,0,490), Position=Focus+(Positions[CameraIndex]-Focus)*CameraZoom;
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
    Water->ClearInstances(); for(const auto& D:Sim.drops) Water->AddInstance(FTransform(WorldPoint(D.v).Rotation(),WorldPoint(D.p),FVector(.21f,.075f,.075f)),true);
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
        Ribbons[I]->SetVisibility(B.alive); Ribbons[I]->SetWorldLocation(Position-FVector(0,0,Size*50+55)); Ribbons[I]->SetWorldRotation(FRotator(FMath::Sin(SceneTime*2+I)*8,0,0));
        if(BalloonMaterials[I]) BalloonMaterials[I]->SetVectorParameterValue(TEXT("Tint"),B.protectedZone?FLinearColor(.22f,.3f,.34f):FMath::Lerp(BalloonColor(I,B.kind),FLinearColor(.25f,.8f,1),float(B.wet*.4)));
    }
    FVector Direction=WorldPoint(Sim.aim); Head->SetWorldRotation(Direction.Rotation()); FRotator BarrelRotation=FRotationMatrix::MakeFromZ(Direction).Rotator(); Barrel->SetWorldRotation(BarrelRotation); Barrel->SetWorldLocation(WorldPoint(Sim.nozzle)+Direction*120); Muzzle->SetWorldRotation(BarrelRotation); Muzzle->SetWorldLocation(WorldPoint(Sim.nozzle)+Direction*222);
    UpdateEffects(Sim.paused?0:Dt); UpdateCamera(Dt);
    if(Sim.wave!=LastWave) { LastWave=Sim.wave; WaveBanner=3; if(bSound && WaveSound) UGameplayStatics::PlaySound2D(this,WaveSound,.3f); }
    if(bDemoTest) {
        DemoWallTime+=Dt;
        if(DemoWallTime>15 && !bScreenshot) { bScreenshot=true; FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("UnrealDemo.png")),false,false); }
        if(DemoWallTime>20) { FString Result=FString::Printf(TEXT("{\"popped\":%d,\"water\":%.2f,\"seconds\":%.2f,\"unreal\":true}"),Sim.popped,Sim.tank,Sim.time); FFileHelper::SaveStringToFile(Result,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("UnrealDemoResult.json"))); FGenericPlatformMisc::RequestExit(false); }
    }
}
ASplashGameMode::ASplashGameMode() { HUDClass=ASplashHUD::StaticClass(); DefaultPawnClass=nullptr; }
void ASplashGameMode::BeginPlay() { Super::BeginPlay(); GetWorld()->SpawnActor<ASplashRange>(); }

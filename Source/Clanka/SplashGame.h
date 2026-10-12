#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "Simulation.h"
#include "SplashGame.generated.h"

class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class ACameraActor;
class USoundBase;
class UAudioComponent;
struct FWaterParticle { FVector Position, Velocity; float Age=0, Life=1, Size=1; int Kind=0; };
struct FScoreFloat { FVector Position; int Points; float Age=0; };

UCLASS()
class CLANKA_API ASplashRange : public AActor {
    GENERATED_BODY()
public:
    ASplashRange();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    splash::Simulation Sim;
    void Command(splash::Action Action);
    void ChangeCamera();
    void ExportLog();
    void Select(int Index);
    void StartGame(bool Autopilot);
    void ReturnToMenu();
    void ToggleSound();
    void ToggleQuality();
    void ApplyQuality();
    FString QualityLabel() const;
    void SaveBest();
    int BestScore=0;
    bool bSound=true, bAutopilot=false;
    bool bPhotoMode=false;
    int GraphicsPreset=1;
    float SmoothedFPS=60;
    float SceneTime=0, WaveBanner=0;
    TArray<FScoreFloat> FloatingScores;
    bool bHelp=true;
    FString Notice;
    float NoticeSeconds=0;
private:
    UStaticMeshComponent* Mesh(const TCHAR* Shape,FVector Position,FVector Scale,FLinearColor Color);
    UPROPERTY() TObjectPtr<UMaterialInterface> Material;
    UPROPERTY() TObjectPtr<UMaterialInterface> GlowMaterial;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> BalloonMeshes;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> BalloonMaterials;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Head;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Barrel;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Water;
    UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Confetti;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Ribbons;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> BalloonKnots;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Muzzle;
    UPROPERTY() TObjectPtr<UAudioComponent> WaterAudio;
    UPROPERTY() TObjectPtr<UAudioComponent> Ambience;
    UPROPERTY() TObjectPtr<USoundBase> PopSound;
    UPROPERTY() TObjectPtr<USoundBase> ConfirmSound;
    UPROPERTY() TObjectPtr<USoundBase> WaveSound;
    UPROPERTY() TObjectPtr<ACameraActor> Camera;
    double Accumulator=0, DemoWallTime=0;
    bool bDemoTest=false, bScreenshot=false;
    int CameraIndex=0, LastPopped=0, LastWave=1;
    uint64 LastPopSerial=0;
    float CameraZoom=1, WaterAudioLevel=0;
    TArray<FWaterParticle> Particles;
    FRandomStream FXRandom{1776};
    bool bBenchmark=false, bMenuCapture=false, bInteractionTest=false;
    double PerfStart=0, LastFrameClock=0, BenchmarkDuration=65;
    double PausedTestTime=0;
    int TestStage=0, BenchmarkStage=0;
    TArray<double> FrameMilliseconds;
    TArray<int> FrameCameras;
    TArray<FString> InteractionFailures;
    UStaticMeshComponent* Glow(const TCHAR* Shape,FVector Position,FVector Size,FLinearColor Color,float Brightness=1);
    void BuildArena();
    void UpdateCamera(float DeltaSeconds);
    void UpdateEffects(float DeltaSeconds);
    void ReadClick();
    void UpdateDiagnostics(float DeltaSeconds);
    void SaveBenchmark();
};

UCLASS()
class CLANKA_API ASplashHUD : public AHUD {
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
    virtual void NotifyHitBoxClick(FName BoxName) override;
private:
    ASplashRange* Range() const;
    float Scale=1, OffsetX=0, OffsetY=0;
    void Text(const FString& S,float X,float Y,FLinearColor Color,float Size=1);
    void Box(float X,float Y,float W,float H,FLinearColor Color);
    void Button(FName ID,const FString& Label,float X,float Y,float W,FLinearColor Color);
    void Line(float X,float Y,float X2,float Y2,FLinearColor Color,float Thickness=1);
};

UCLASS()
class CLANKA_API ASplashGameMode : public AGameModeBase {
    GENERATED_BODY()
public:
    ASplashGameMode();
    virtual void BeginPlay() override;
};

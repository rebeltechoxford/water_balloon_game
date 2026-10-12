#include "SplashGame.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "Engine/Font.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

static const FLinearColor HUDInk(.012f,.018f,.021f,.86f), HUDPanel(.018f,.025f,.027f,.74f), HUDMint(.58f,.87f,.72f,1), HUDMuted(.57f,.72f,.8f,1), HUDWhite(.94f,.98f,1,1), HUDGold(1,.72f,.3f,1), HUDRed(1,.35f,.32f,1);
ASplashRange* ASplashHUD::Range() const { for(TActorIterator<ASplashRange> I(GetWorld());I;++I) return *I; return nullptr; }
void ASplashHUD::Text(const FString& S,float X,float Y,FLinearColor C,float Size) {
    // Rasterize at the actual display size instead of magnifying a tiny bitmap.
    const float Points=FMath::Max(8.f,float(FMath::RoundToInt(11.f*Size*Scale)));
    const FSlateFontInfo Font(GEngine->GetMediumFont(),Points,Size>=1.3f?TEXT("Bold"):TEXT("Regular"));
    FCanvasTextItem Item(FVector2D(FMath::RoundToInt(OffsetX+X*Scale),FMath::RoundToInt(OffsetY+Y*Scale)),FText::FromString(S),Font,C);
    Canvas->DrawItem(Item);
}
void ASplashHUD::Box(float X,float Y,float W,float H,FLinearColor C) { DrawRect(C,OffsetX+X*Scale,OffsetY+Y*Scale,W*Scale,H*Scale); }
void ASplashHUD::Line(float X,float Y,float X2,float Y2,FLinearColor C,float T) { DrawLine(OffsetX+X*Scale,OffsetY+Y*Scale,OffsetX+X2*Scale,OffsetY+Y2*Scale,C,T*Scale); }
void ASplashHUD::Button(FName ID,const FString& Label,float X,float Y,float W,FLinearColor C) {
    float MX=0,MY=0; GetOwningPlayerController()->GetMousePosition(MX,MY);
    const bool Hover=MX>OffsetX+X*Scale && MX<OffsetX+(X+W)*Scale && MY>OffsetY+Y*Scale && MY<OffsetY+(Y+48)*Scale;
    Box(X,Y,W,48,Hover?FLinearColor(.1f,.17f,.15f,.98f):FLinearColor(.028f,.043f,.045f,.95f)); Box(X,Y,3,48,C);
    Text(Label,X+18,Y+14,C,.89f); AddHitBox(FVector2D(OffsetX+X*Scale,OffsetY+Y*Scale),FVector2D(W*Scale,48*Scale),ID,true);
}
void ASplashHUD::DrawHUD() {
    Super::DrawHUD(); auto* R=Range(); if(!Canvas || !R) return; auto& S=R->Sim;
    Scale=FMath::Min(Canvas->SizeX/1600.f,Canvas->SizeY/900.f); OffsetX=(Canvas->SizeX-1600*Scale)/2; OffsetY=(Canvas->SizeY-900*Scale)/2;
    if(R->bPhotoMode) { Text(TEXT("TAB  RETURN TO CONTROLS"),36,864,HUDWhite,.65f); return; }
    Box(28,22,1544,68,HUDInk); Box(48,39,3,34,HUDMint);
    Text(TEXT("CLANKA"),67,30,HUDWhite,1.75f);
    Text(TEXT("FIELD STATION  /  AUTONOMOUS WATER RANGE"),265,45,HUDMuted,.73f);
    Text(FString::Printf(TEXT("%03d   POPPED"),S.popped),1045,43,HUDWhite,.92f);
    Text(FString::Printf(TEXT("%02d:%02d"),int(S.time)/60,int(S.time)%60),1265,43,HUDMuted,.92f);
    Text(FString::Printf(TEXT("%.0f FPS"),R->SmoothedFPS),1436,43,R->SmoothedFPS>=29.5f?HUDMint:HUDGold,.85f);
    if(!S.running) {
        Box(64,236,609,441,HUDInk); Box(64,236,3,441,HUDMint);
        Text(TEXT("FIELD TRIAL 03  /  THE WATER WORKS"),101,268,HUDGold,.82f);
        Text(TEXT("JUST ADD"),98,314,HUDWhite,3.35f);
        Text(TEXT("WATER."),98,375,HUDWhite,3.35f);
        Text(TEXT("Switch ON. Engage. Watch it work."),101,462,HUDWhite,1.08f);
        Text(TEXT("Endless balloons. Automatic water. Your front-row seat."),101,504,HUDMuted,.73f);
        Button(S.stopped?TEXT("release"):TEXT("on"),S.stopped?TEXT("RELEASE STOP TO RESTART"):TEXT("POWER ON   /   CLICK TO START"),101,557,527,S.stopped?HUDRed:HUDMint);
        Text(TEXT("C  CHANGE CAMERA     SCROLL  ZOOM     TAB  PHOTO VIEW"),101,636,HUDMuted,.62f);
        Button(TEXT("quality"),TEXT("GRAPHICS: ")+R->QualityLabel(),64,785,326,HUDMuted);
        Button(TEXT("sound"),R->bSound?TEXT("SOUND ON"):TEXT("SOUND OFF"),406,785,181,HUDMuted);
        Button(TEXT("quit"),TEXT("QUIT"),603,785,118,HUDMuted);
        Text(TEXT("A SMALL MACHINE. A SATISFYING JOB."),1060,844,HUDWhite,.78f);
        return;
    }
    Box(28,111,287,152,HUDPanel); Text(TEXT("SYSTEM STATUS"),48,129,HUDMuted,.6f);
    Text(UTF8_TO_TCHAR(S.status()),48,160,S.stopped?HUDRed:(S.verified?HUDMint:HUDGold),.94f);
    const auto* T=S.target();
    Text(T?FString::Printf(TEXT("TRACK B-%02d   /   %.0f%% ACQUIRED"),T->id,T->confidence*100):TEXT("SEARCHING THE RANGE"),48,200,HUDWhite,.62f);
    const TCHAR* Wind[]={TEXT("CALM"),TEXT("BREEZE"),TEXT("GUSTS")};
    Text(FString::Printf(TEXT("WIND  %s    /    1  2  3"),Wind[S.scenario]),48,232,HUDMuted,.61f);
    Box(1320,111,252,114,HUDPanel); Text(TEXT("WATER RESERVE"),1340,129,HUDMuted,.6f);
    Text(FString::Printf(TEXT("%.0f%%"),S.tank),1340,153,HUDWhite,1.6f);
    Box(1340,196,210,4,HUDInk); Box(1340,196,210*S.tank/100,4,HUDMint);
    Text(S.refill>0?TEXT("AUTOMATIC REFILL IN PROGRESS"):TEXT("AUTO REFILL ENABLED"),1324,235,HUDWhite,.61f);
    if(!S.verified && !S.stopped && !S.paused) {
        Box(485,118,630,65,HUDInk); Text(TEXT("TRACKING ONLINE  /  READY TO ENGAGE"),516,132,HUDMint,.86f);
        Text(TEXT("Press ENGAGE below to start automatic spraying."),516,160,HUDMuted,.63f);
    }
    // Every annotation is projected from live backend positions.
    TArray<FVector2D> UsedLabels;
    for(int I=0;I<static_cast<int>(S.balloons.size());++I) {
        const auto& B=S.balloons[I]; if(!B.alive) continue; FVector2D P;
        if(!GetOwningPlayerController()->ProjectWorldLocationToScreen(FVector(B.p.x,B.p.y,B.p.z),P)) continue;
        float X=(P.X-OffsetX)/Scale,Y=(P.Y-OffsetY)/Scale; if(X<325 || X>1305 || Y<192 || Y>750) continue;
        const bool Selected=I==S.selected; FLinearColor C=B.protectedZone?HUDGold:(Selected?HUDMint:HUDWhite);
        if(Selected) {
            constexpr float D=42,L=13;
            Line(X-D,Y-D,X-D+L,Y-D,C,2); Line(X-D,Y-D,X-D,Y-D+L,C,2);
            Line(X+D,Y-D,X+D-L,Y-D,C,2); Line(X+D,Y-D,X+D,Y-D+L,C,2);
            Line(X-D,Y+D,X-D+L,Y+D,C,2); Line(X+D,Y+D,X+D-L,Y+D,C,2);
            Box(X-34,Y+50,68,4,HUDInk); Box(X-34,Y+50,68*B.wet,4,HUDMint);
        }
        // Only label selected/protected tracks; other balloons keep the view clear.
        if(Selected || B.protectedZone) {
            float LY=Y-67; for(const auto& Existing:UsedLabels) if(FMath::Abs(Existing.X-X)<125 && FMath::Abs(Existing.Y-LY)<22) LY-=24;
            UsedLabels.Add(FVector2D(X,LY));
            Text(FString::Printf(TEXT("B-%02d  %s"),B.id,B.protectedZone?TEXT("ZONE"):TEXT("LOCK")),X-40,LY,C,.63f);
        }
    }
    for(const auto& F:R->FloatingScores) { FVector2D P; if(GetOwningPlayerController()->ProjectWorldLocationToScreen(F.Position,P)) { FLinearColor C=HUDMint; C.A=FMath::Clamp(1-F.Age/1.15f,0.f,1.f); Text(TEXT("SPLASH!"),(P.X-OffsetX)/Scale-38,(P.Y-OffsetY)/Scale-20,C,1.0f); } }
    Box(28,783,1544,94,HUDInk);
    Button(TEXT("engage"),S.verified?TEXT("DISENGAGE  /  V"):TEXT("ENGAGE  /  ENTER"),46,800,252,S.verified?HUDGold:HUDMint);
    Button(TEXT("camera"),TEXT("CAMERA  /  C"),310,800,182,HUDMuted);
    Button(TEXT("pause"),S.paused?TEXT("RESUME"):TEXT("PAUSE"),504,800,132,HUDMuted);
    Button(TEXT("stop"),S.stopped?TEXT("RELEASE STOP"):TEXT("STOP  /  E"),648,800,183,HUDRed);
    Button(TEXT("quality"),R->QualityLabel(),843,800,203,HUDMuted);
    Button(TEXT("off"),TEXT("POWER OFF"),1058,800,172,HUDMuted);
    Button(TEXT("sound"),R->bSound?TEXT("SOUND ON"):TEXT("SOUND OFF"),1242,800,158,HUDMuted);
    Button(TEXT("help"),TEXT("?"),1412,800,48,HUDMuted);
    Button(TEXT("reset"),TEXT("RESET"),1472,800,82,HUDMuted);
    Text(TEXT("TAB  PHOTO VIEW    /    SCROLL  ZOOM    /    CLICK A BALLOON TO PRIORITIZE"),48,855,HUDMuted,.55f);
    Text(TEXT("AMBER LANE: NO SPRAY"),1320,855,HUDGold,.55f);
    if(R->NoticeSeconds>0) { Box(444,707,713,44,HUDPanel); Text(R->Notice,460,721,HUDMint,.7f); }
    if(S.stopped || S.paused) {
        Box(516,328,568,158,HUDInk); Text(S.stopped?TEXT("SYSTEM STOPPED"):TEXT("SIMULATION PAUSED"),552,355,S.stopped?HUDRed:HUDWhite,1.4f);
        Text(S.stopped?TEXT("Release the stop, then ENGAGE to restart water."):TEXT("Press Space or RESUME to continue."),552,412,HUDMuted,.8f);
    }
    if(R->bHelp) {
        Box(407,235,786,363,HUDInk); Text(TEXT("AUTONOMOUS BY DESIGN"),447,268,HUDMint,1.4f);
        Text(TEXT("ON starts tracking. ENGAGE starts the water."),447,328,HUDWhite,.96f);
        Text(TEXT("The cannon chooses a balloon, aligns, and sprays it."),447,366,HUDWhite,.92f);
        Text(TEXT("Amber zone balloons are excluded. Refills are automatic."),447,404,HUDWhite,.9f);
        Text(TEXT("C  camera    Q  graphics    Tab  photo view    M  sound"),447,461,HUDMuted,.82f);
        Text(TEXT("L  save log     R  reset     Esc  power off / quit"),447,498,HUDMuted,.82f);
        Button(TEXT("close_help"),TEXT("GOT IT"),447,543,220,HUDMint);
    }
}
void ASplashHUD::NotifyHitBoxClick(FName Name) {
    auto* R=Range(); if(!R) return; using A=splash::Action;
    if(Name==TEXT("on")) R->StartGame(true);
    else if(Name==TEXT("release")) R->Command(A::ReleaseStop);
    else if(Name==TEXT("engage")) R->Command(R->Sim.verified?A::Verify:A::Engage);
    else if(Name==TEXT("pause")) R->Command(A::Pause);
    else if(Name==TEXT("stop")) R->Command(R->Sim.stopped?A::ReleaseStop:A::Stop);
    else if(Name==TEXT("off")) R->ReturnToMenu();
    else if(Name==TEXT("camera")) R->ChangeCamera();
    else if(Name==TEXT("quality")) R->ToggleQuality();
    else if(Name==TEXT("sound")) R->ToggleSound();
    else if(Name==TEXT("reset")) R->Command(A::Reset);
    else if(Name==TEXT("help")) R->bHelp=!R->bHelp;
    else if(Name==TEXT("close_help")) R->bHelp=false;
    else if(Name==TEXT("quit")) FGenericPlatformMisc::RequestExit(false);
}

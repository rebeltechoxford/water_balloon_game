#include "SplashGame.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/RotationMatrix.h"

void ASplashRange::BuildArena()
{
    // Centimetres throughout. The playable pad stays open; detail sits along its
    // edges. Repeated scenery shares meshes/materials instead of separate draws.
    FRandomStream Scenery(7319);
    auto Surface = [this](const TCHAR* Name, FLinearColor Tint, float Roughness, float Tile)
    {
        const FString Path = FString::Printf(TEXT("/Game/Realism/%s.%s"), Name, Name);
        UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, *Path);
        if (!Base) Base = Material.Get();
        UMaterialInstanceDynamic* Result = UMaterialInstanceDynamic::Create(Base, this);
        if (Result)
        {
            Result->SetVectorParameterValue(TEXT("Tint"), Tint);
            Result->SetScalarParameterValue(TEXT("Roughness"), Roughness);
            Result->SetScalarParameterValue(TEXT("WorldScale"), Tile);
        }
        return Result;
    };
    auto* Concrete = Surface(TEXT("M_Concrete"), FLinearColor(.82f,.82f,.77f), .92f, 240);
    auto* PaleConcrete = Surface(TEXT("M_Concrete"), FLinearColor(.62f,.64f,.62f), .94f, 110);
    auto* Asphalt = Surface(TEXT("M_Asphalt"), FLinearColor(.77f,.77f,.75f), .94f, 550);
    auto* Meadow = Surface(TEXT("M_Meadow"), FLinearColor(.7f,.85f,.7f), 1, 200);
    auto* Rock = Surface(TEXT("M_Rock"), FLinearColor(.78f,.75f,.64f), .98f, 220);
    auto* Steel = Surface(TEXT("M_Metal"), FLinearColor(.29f,.34f,.35f), .74f, 100);
    auto* Galvanized = Surface(TEXT("M_Metal"), FLinearColor(.52f,.55f,.54f), .63f, 100);
    auto* DarkMetal = Surface(TEXT("M_Metal"), FLinearColor(.055f,.065f,.07f), .82f, 100);
    auto* Paint = Surface(TEXT("M_PaintedMetal"), FLinearColor(.20f,.33f,.37f), .72f, 100);
    auto* Safety = Surface(TEXT("M_PaintedMetal"), FLinearColor(.73f,.49f,.075f), .88f, 100);
    auto* WhitePaint = Surface(TEXT("M_PaintedMetal"), FLinearColor(.63f,.63f,.56f), .90f, 100);
    auto* RedPaint = Surface(TEXT("M_PaintedMetal"), FLinearColor(.40f,.075f,.045f), .8f, 100);
    auto* Rust = Surface(TEXT("M_Metal"), FLinearColor(.22f,.13f,.075f), .96f, 100);
    auto* WaterSurface = Surface(TEXT("M_Water"), FLinearColor(.015f,.035f,.035f), .12f, 120);
    if (WaterSurface) WaterSurface->SetScalarParameterValue(TEXT("Opacity"), .42f);
    auto* Glass = Surface(TEXT("M_Glass"), FLinearColor(.12f,.20f,.21f), .22f, 100);
    auto* Grass = Surface(TEXT("M_Rock"), FLinearColor(.19f,.23f,.10f), 1, 60);
    auto* DryGrass = Surface(TEXT("M_Rock"), FLinearColor(.39f,.34f,.19f), 1, 60);
    for (auto* Painted : {Paint, Safety, WhitePaint, RedPaint})
        if (Painted) Painted->SetScalarParameterValue(TEXT("Metallic"), .08f);

    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UStaticMesh* RockMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Realism/SM_Rock.SM_Rock"));
    UStaticMesh* TreeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Realism/SM_Tree.SM_Tree"));
    UStaticMesh* GrassMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Realism/SM_Grass.SM_Grass"));
    // A separate batch is deliberate when a material or shadow policy differs.
    auto Batch = [this](UStaticMesh* Shape, UMaterialInterface* Finish, bool Shadows=true, int32 Cull=0)
    {
        auto* Instances = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
        Instances->SetupAttachment(RootComponent);
        Instances->SetMobility(EComponentMobility::Movable);
        Instances->SetStaticMesh(Shape);
        Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Instances->SetCastShadow(Shadows);
        Instances->bAffectDistanceFieldLighting = false;
        Instances->SetCanEverAffectNavigation(false);
        if (Finish) Instances->SetMaterial(0, Finish);
        if (Cull > 0) Instances->SetCullDistances(Cull*3/4, Cull);
        Instances->RegisterComponent();
        return Instances;
    };
    auto Put = [](UHierarchicalInstancedStaticMeshComponent* B, FVector P, FVector Dimensions, FRotator R=FRotator::ZeroRotator)
    {
        B->AddInstance(FTransform(R, P, Dimensions/100.0), true);
    };
    auto Pipe = [&Put](UHierarchicalInstancedStaticMeshComponent* B, FVector A, FVector Z, float Diameter)
    {
        const FVector Direction = Z-A;
        Put(B, (A+Z)*.5, FVector(Diameter,Diameter,Direction.Size()), FRotationMatrix::MakeFromZ(Direction).Rotator());
    };
    auto* ConcreteBlocks = Batch(Cube, Concrete);
    auto* CurbBlocks = Batch(Cube, PaleConcrete);
    auto* Ground = Batch(Cube, Meadow, false);
    auto* Road = Batch(Cube, Asphalt, false);
    auto* MetalBoxes = Batch(Cube, Steel);
    auto* LightMetalBoxes = Batch(Cube, Galvanized);
    auto* DarkBoxes = Batch(Cube, DarkMetal);
    auto* RustBoxes = Batch(Cube, Rust);
    auto* PaintedBoxes = Batch(Cube, Paint);
    auto* YellowBoxes = Batch(Cube, Safety, false, 12000);
    auto* WhiteBoxes = Batch(Cube, WhitePaint, false, 10000);
    auto* Pipes = Batch(Cylinder, Galvanized);
    auto* DarkPipes = Batch(Cylinder, DarkMetal);
    auto* PaintedPipes = Batch(Cylinder, Paint);
    auto* Bolts = Batch(Cylinder, Galvanized, false, 4500);
    auto* Grating = Batch(Cube, DarkMetal, false, 8500);
    auto* Puddles = Batch(Cylinder, WaterSurface, false, 11000);

    // An uninterrupted 400-metre ground plane prevents a floating arena horizon.
    Put(Ground, FVector(4000,3000,-145), FVector(40000,40000,250));
    Put(Road, FVector(650,-100,-33), FVector(7000,6700,40));
    Put(ConcreteBlocks, FVector(1250,0,-15), FVector(4300,3100,30));
    // Square pours and recessed joints break up the otherwise large surface.
    for (int I=0; I<8; ++I)
        Put(DarkBoxes, FVector(-550+I*525,0,.35f), FVector(.9f,3070,.6f));
    for (int I=-2; I<=2; ++I)
        Put(DarkBoxes, FVector(1250,I*510,.4f), FVector(4270,.8f,.6f));
    for (int Side : {-1,1})
    {
        for (int I=0; I<18; ++I)
            Put(CurbBlocks, FVector(-835+I*245,Side*1580,8), FVector(238,42,35));
        Put(DarkBoxes, FVector(1210,Side*1507,1), FVector(4110,34,7));
        for (int I=0; I<169; ++I)
            Put(Grating, FVector(-830+I*24.3f,Side*1507,5.5f), FVector(5,30,4));
        Pipe(Pipes, FVector(-850,Side*1492,5), FVector(3250,Side*1492,5), 2.4f);
        Pipe(Pipes, FVector(-850,Side*1522,5), FVector(3250,Side*1522,5), 2.4f);
    }
    // Subdued paint is readable without glowing or covering the test surface.
    for (int I=0; I<19; ++I)
        Put(YellowBoxes, FVector(125+I*152,620,1), FVector(93,8,.7f));
    for (int I=0; I<18; ++I)
        Put(YellowBoxes, FVector(60+I*168,1335,1), FVector(65,8,.7f), FRotator(0,-42,0));
    Put(WhiteBoxes, FVector(1130,-1280,1), FVector(3050,8,.7f));
    Put(WhiteBoxes, FVector(2920,-570,1), FVector(8,1420,.7f));
    for (int I=0; I<7; ++I)
        Put(WhiteBoxes, FVector(300+I*360,-1215,1.1f), FVector(7,100,.8f));
    // Shallow puddles sit by drainage, leaving the center mostly dry.
    Put(Puddles, FVector(2530,-1370,1.3f), FVector(380,112,1), FRotator(0,-13,0));
    Put(Puddles, FVector(2790,-1385,1.6f), FVector(220,125,1), FRotator(0,12,0));
    Put(Puddles, FVector(3150,1200,1.4f), FVector(270,185,1), FRotator(0,-28,0));
    Put(Puddles, FVector(-610,370,1.3f), FVector(155,90,1), FRotator(0,32,0));

    // A low workshop frames the rear corner without obscuring the balloon field.
    Put(ConcreteBlocks, FVector(720,2940,18), FVector(2750,1140,62));
    Put(PaintedBoxes, FVector(720,2940,340), FVector(2550,980,650));
    Put(DarkBoxes, FVector(720,2940,678), FVector(2700,1100,35));
    for (int I=0; I<66; ++I)
        Put(LightMetalBoxes, FVector(-535+I*38,2438,335), FVector(4,8,628));
    // Roller door, lintel and individual slats create believable construction.
    Put(DarkBoxes, FVector(1140,2425,237), FVector(560,25,420));
    for (int I=0; I<26; ++I)
        Put(LightMetalBoxes, FVector(1140,2407,43+I*15.2f), FVector(510,13,13));
    Put(MetalBoxes, FVector(1140,2397,455), FVector(588,35,42));
    for (int Side : {-1,1})
        Put(MetalBoxes, FVector(1140+Side*277,2396,235), FVector(24,26,430));
    Put(DarkBoxes, FVector(-285,2422,158), FVector(132,25,273));
    Put(LightMetalBoxes, FVector(-285,2405,157), FVector(115,9,257));
    Put(DarkBoxes, FVector(-245,2397,151), FVector(5,8,32));
    auto* Windows = Batch(Cube, Glass, false);
    for (int I=0; I<3; ++I)
    {
        const float X=-40+I*265;
        Put(DarkBoxes, FVector(X,2420,444), FVector(195,21,163));
        Put(Windows, FVector(X,2404,444), FVector(172,5,141));
        Put(LightMetalBoxes, FVector(X,2399,444), FVector(6,9,151));
        Put(LightMetalBoxes, FVector(X,2399,444), FVector(180,9,6));
    }
    Pipe(Pipes, FVector(-515,2438,40), FVector(-515,2438,640), 12);
    Pipe(Pipes, FVector(-515,2438,640), FVector(1950,2438,640), 12);
    // Rooftop extractor, utility cabinet, service conduit and side access step.
    Put(MetalBoxes, FVector(1630,2960,767), FVector(370,280,155));
    for (int I=0; I<9; ++I)
        Put(DarkBoxes, FVector(1474+I*37,2815,763), FVector(10,5,107));
    Put(ConcreteBlocks, FVector(-290,2320,5), FVector(230,150,22));
    Put(LightMetalBoxes, FVector(2380,2180,123), FVector(160,95,238));
    for (int I=0; I<8; ++I)
        Put(DarkBoxes, FVector(2380,2129,180-I*11), FVector(97,2,4));
    Pipe(Pipes, FVector(2340,2200,0), FVector(2340,2200,350), 7);
    Pipe(Pipes, FVector(2340,2200,350), FVector(1955,2460,350), 7);

    // Galvanized perimeter railings: only rear and right boundaries are tall.
    auto FenceRun = [&](FVector A, FVector B)
    {
        FVector Direction=(B-A).GetSafeNormal();
        const float Length=(B-A).Size();
        const int Panels=FMath::CeilToInt(Length/285);
        for (int I=0; I<=Panels; ++I)
        {
            const FVector P=FMath::Lerp(A,B,float(I)/Panels);
            Pipe(Pipes,P,P+FVector(0,0,190),7);
            Put(CurbBlocks,P+FVector(0,0,-1),FVector(40,40,22));
            Put(Bolts,P+FVector(0,0,195),FVector(9,9,5));
        }
        for (float Height : {35.f,110.f,182.f})
            Pipe(Pipes,A+FVector(0,0,Height),B+FVector(0,0,Height),4);
        for (float Along=20; Along<Length; Along+=36)
        {
            FVector P=A+Direction*Along;
            Pipe(DarkPipes,P+FVector(0,0,37),P+FVector(0,0,180),2.2f);
        }
    };
    FenceRun(FVector(3510,-2040,0),FVector(3510,3480,0));
    FenceRun(FVector(-1920,3490,0),FVector(3510,3490,0));
    FenceRun(FVector(-1750,1450,0),FVector(-1750,3490,0));
    // Road markings, concrete wheel stops and worn bollards on the service side.
    for (int I=0; I<4; ++I)
    {
        Put(CurbBlocks,FVector(-1240,-1040+I*440,4),FVector(48,290,24));
        Put(YellowBoxes,FVector(-1650,-1250+I*440,-11),FVector(650,9,.8f));
    }
    for (const FVector P : {FVector(-930,720,0),FVector(-930,280,0),FVector(-360,900,0),FVector(2200,2080,0)})
    {
        Put(PaintedPipes,P+FVector(0,0,57),FVector(19,19,114));
        Put(YellowBoxes,P+FVector(0,0,73),FVector(21,21,18));
        Put(CurbBlocks,P+FVector(0,0,4),FVector(42,42,15));
    }

    // Tank, pump skid, hoses, flanges and valve gear around the cannon station.
    const FVector Tank(-620,610,188);
    Put(ConcreteBlocks,FVector(-620,610,12),FVector(430,400,30));
    Put(PaintedPipes,Tank,FVector(255,255,348));
    Put(Pipes,Tank+FVector(0,0,174),FVector(264,264,12));
    Put(Pipes,Tank-FVector(0,0,169),FVector(267,267,15));
    Put(DarkPipes,Tank+FVector(0,0,188),FVector(84,84,22));
    for (float Height : {72.f,210.f,324.f})
        Put(Pipes,FVector(-620,610,Height),FVector(259,259,8));
    // Ladder is one steel batch, its rungs kept outside the active tracking arc.
    Pipe(Pipes,FVector(-760,568,35),FVector(-760,568,403),4);
    Pipe(Pipes,FVector(-760,650,35),FVector(-760,650,403),4);
    for (int I=0; I<12; ++I)
        Pipe(Pipes,FVector(-760,568,43+I*30),FVector(-760,650,43+I*30),3);
    Put(MetalBoxes,FVector(-525,238,24),FVector(275,128,35));
    Put(PaintedBoxes,FVector(-520,230,76),FVector(165,102,86));
    Put(DarkPipes,FVector(-610,230,91),FVector(77,77,125),FRotator(0,0,90));
    for (int I=0; I<8; ++I)
        Put(Pipes,FVector(-610,197+I*9,91),FVector(80,80,3),FRotator(0,0,90));
    const FVector Plumbing[]={FVector(-480,610,95),FVector(-380,610,95),FVector(-380,230,95),FVector(-330,230,70),FVector(-80,230,70),FVector(-80,0,70),FVector(0,0,100)};
    for (int I=0; I<6; ++I)
    {
        Pipe(PaintedPipes,Plumbing[I],Plumbing[I+1],24);
        const FVector Axis=(Plumbing[I+1]-Plumbing[I]).GetSafeNormal();
        Pipe(Pipes,Plumbing[I]+Axis*8,Plumbing[I]+Axis*16,37);
    }
    // Valve wheel and pressure gauge, useful familiar details at the control end.
    auto* Valve = Mesh(TEXT("Cylinder"),FVector(-380,406,134),FVector(.42f,.42f,.04f),FLinearColor(.4f,.06f,.025f));
    Valve->SetMaterial(0,RedPaint);
    Put(DarkPipes,FVector(-380,406,115),FVector(8,8,37));
    Put(Pipes,FVector(-490,186,145),FVector(35,35,12),FRotator(0,0,90));
    Put(WhiteBoxes,FVector(-490,178,145),FVector(26,2,26));
    Put(DarkBoxes,FVector(-490,176,148),FVector(2,2,12),FRotator(0,0,-24));

    // Compact physical mount. Only these three components are driven by the sim.
    const FVector Pivot(Sim.nozzle.x,Sim.nozzle.y,Sim.nozzle.z);
    Put(ConcreteBlocks,FVector(0,0,12),FVector(210,210,24));
    Put(MetalBoxes,FVector(0,0,29),FVector(145,145,14));
    Put(Pipes,FVector(0,0,58),FVector(120,120,42));
    Put(DarkPipes,FVector(0,0,85),FVector(109,109,13));
    for (int Side : {-1,1})
    {
        Put(PaintedBoxes,FVector(0,Side*62,126),FVector(71,22,92));
        Pipe(Pipes,FVector(0,Side*47,Pivot.Z),FVector(0,Side*82,Pivot.Z),35);
    }
    for (int X : {-1,1}) for (int Y : {-1,1})
        Put(Bolts,FVector(X*58,Y*58,41),FVector(15,15,13));
    Head=Mesh(TEXT("Cube"),Pivot,FVector(.88f,1.02f,.74f),FLinearColor(.31f,.43f,.43f));
    Head->SetMaterial(0,Paint);
    Barrel=Mesh(TEXT("Cylinder"),Pivot,FVector(.38f,.38f,2),FLinearColor(.35f,.4f,.4f));
    Barrel->SetMaterial(0,Galvanized);
    Muzzle=Mesh(TEXT("Cylinder"),Pivot,FVector(.47f,.47f,.12f),FLinearColor(.05f,.055f,.058f));
    Muzzle->SetMaterial(0,DarkMetal);
    // Clamp rings attach with their own absolute scale preserved when the barrel aims.
    for (float Offset : {-75.f,55.f,91.f})
    {
        auto* Clamp=Mesh(TEXT("Cylinder"),Pivot+FVector(0,0,Offset),FVector(.45f,.45f,.085f),FLinearColor(.16f,.18f,.18f));
        Clamp->SetMaterial(0,DarkMetal);
        Clamp->AttachToComponent(Barrel,FAttachmentTransformRules::KeepWorldTransform);
    }
    auto* Aperture=Mesh(TEXT("Cylinder"),Pivot+FVector(0,0,7),FVector(.32f,.32f,.015f),FLinearColor(.012f,.014f,.015f));
    Aperture->SetMaterial(0,DarkMetal);
    Aperture->AttachToComponent(Muzzle,FAttachmentTransformRules::KeepWorldTransform);
    // A tiny status lamp, rather than area-wide emissive strips.
    Glow(TEXT("Sphere"),FVector(-405,171,119),FVector(.045f),FLinearColor(.14f,.46f,.25f),.6f);

    // Floodlight housings add an industrial silhouette without dynamic point lights.
    for (int Side : {-1,1})
    {
        const FVector P(3100,Side*1940,0);
        Put(CurbBlocks,P+FVector(0,0,15),FVector(90,90,50));
        Pipe(Pipes,P+FVector(0,0,25),P+FVector(0,0,650),13);
        Pipe(Pipes,P+FVector(0,0,646),P+FVector(-100,0,646),9);
        Put(DarkBoxes,P+FVector(-120,0,631),FVector(75,52,35),FRotator(0,0,-12));
        Put(WhiteBoxes,P+FVector(-129,0,613),FVector(63,43,3),FRotator(0,0,-12));
    }
    auto Sign = [this](const TCHAR* Words,FVector Position,float Size,FColor Color)
    {
        auto* Label=NewObject<UTextRenderComponent>(this);
        Label->SetupAttachment(RootComponent);
        Label->SetWorldSize(Size);
        Label->SetHorizontalAlignment(EHTA_Center);
        Label->SetVerticalAlignment(EVRTA_TextCenter);
        Label->SetText(FText::FromString(Words));
        Label->SetTextRenderColor(Color);
        Label->SetWorldLocation(Position);
        Label->SetWorldRotation(FRotator(0,-90,0));
        Label->SetCastShadow(false);
        Label->RegisterComponent();
    };
    Put(DarkBoxes,FVector(500,2398,565),FVector(1250,9,94));
    Sign(TEXT("REBEL TECH  /  WATER TEST FACILITY"),FVector(500,2390,565),37,FColor(178,183,171));
    Put(YellowBoxes,FVector(2660,1920,144),FVector(238,9,86));
    Sign(TEXT("TEST BAY 02"),FVector(2660,1913,144),27,FColor(37,39,32));
    Pipe(Pipes,FVector(2660,1920,0),FVector(2660,1920,185),6);

    // Service details live beside the equipment, never in the open spray area.
    for (int Turn=0; Turn<3; ++Turn)
    {
        const float Radius=62+Turn*12;
        for (int Segment=0; Segment<24; ++Segment)
        {
            const float A=Segment*2*PI/24, B=(Segment+1)*2*PI/24;
            Pipe(DarkPipes,FVector(-620+FMath::Cos(A)*Radius,-170+FMath::Sin(A)*Radius,7),
                FVector(-620+FMath::Cos(B)*Radius,-170+FMath::Sin(B)*Radius,7),8);
        }
    }
    auto* Timber=Surface(TEXT("M_Concrete"),FLinearColor(.41f,.31f,.18f),1,70);
    auto* PalletBoards=Batch(Cube,Timber);
    for (int Level=0; Level<3; ++Level)
    {
        const float Z=12+Level*16;
        for (int Rail=0; Rail<3; ++Rail)
            Put(PalletBoards,FVector(2040,2010+Rail*45,Z),FVector(128,11,10));
        for (int Board=0; Board<7; ++Board)
            Put(PalletBoards,FVector(1986+Board*18,2055,Z+7),FVector(13,120,5));
    }
    Put(PaintedBoxes,FVector(2040,2055,92),FVector(100,90,65));
    Put(DarkBoxes,FVector(2040,2055,126),FVector(105,95,5));

    // Woodland grows in overlapping copses instead of isolated trees and giant
    // boulders sprinkled across a bare plane. The nearest group sits beyond
    // the fence; gaps between groups preserve a readable natural horizon.
    const FVector Copses[]={
        FVector(5700,-1700,0),FVector(7300,1400,0),FVector(6700,4500,0),
        FVector(9800,6500,0),FVector(3900,7200,0),FVector(-300,7900,0),
        FVector(-4300,7300,0),FVector(-6400,3300,0),FVector(-7200,-3200,0),
        FVector(11800,-4600,0),FVector(14200,1100,0)};
    auto* Rocks = Batch(RockMesh ? RockMesh : Cube, RockMesh ? nullptr : Rock);
    FVector RockSize = RockMesh ? RockMesh->GetBounds().BoxExtent*2 : FVector(100);
    for (int Group=0; Group<8; ++Group)
    {
        const FVector Anchor=Copses[Group]+FVector(-530,-240,-47);
        for (int I=0; I<3; ++I)
        {
            const FVector P=Anchor+FVector(Scenery.FRandRange(-260,260),Scenery.FRandRange(-200,200),Scenery.FRandRange(-12,8));
            const float Width=Scenery.FRandRange(140,360);
            const FVector Dimensions(Width,Width*Scenery.FRandRange(.7f,1.15f),Scenery.FRandRange(65,155));
            const FVector Scale(Dimensions.X/FMath::Max(1.0,RockSize.X),Dimensions.Y/FMath::Max(1.0,RockSize.Y),Dimensions.Z/FMath::Max(1.0,RockSize.Z));
            Rocks->AddInstance(FTransform(FRotator(Scenery.FRandRange(-9,9),Scenery.FRandRange(0,360),Scenery.FRandRange(-7,7)),P,Scale),true);
        }
    }
    // Three small groups are partly buried in the verge, not on the asphalt.
    for (int I=0; I<9; ++I)
    {
        const FVector Anchors[]={FVector(5530,-1800,-38),FVector(2600,3740,-38),FVector(-3650,3660,-38)};
        const FVector P=Anchors[I/3]+FVector(Scenery.FRandRange(-120,120),Scenery.FRandRange(-90,90),0);
        const float S=Scenery.FRandRange(65,175);
        Rocks->AddInstance(FTransform(FRotator(0,Scenery.FRandRange(0,360),6),P,FVector(S/RockSize.X,S*.82/RockSize.Y,S*.48/RockSize.Z)),true);
    }
    if (TreeMesh)
    {
        // Keep authored bark/leaf slots on an optional vegetation mesh.
        auto* Trees = Batch(TreeMesh,nullptr,true,33000);
        const float Height=FMath::Max(1.0,TreeMesh->GetBounds().BoxExtent.Z*2);
        for (int Group=0; Group<11; ++Group)
        {
            for (int I=0; I<10; ++I)
            {
                const float Angle=Scenery.FRandRange(0,2*PI),Radius=Scenery.FRandRange(220,1300);
                const FVector P=Copses[Group]+FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,-28);
                const float Scale=Scenery.FRandRange(640,1230)/Height;
                Trees->AddInstance(FTransform(FRotator(Scenery.FRandRange(-2,2),Scenery.FRandRange(0,360),0),P,FVector(Scale,Scale,Scale*Scenery.FRandRange(.95f,1.12f))),true);
            }
        }
    }
    // Imported foliage keeps its alpha-masked material; opaque rock materials
    // are only a fallback for the narrow geometric blades.
    auto* GreenBlades = Batch(GrassMesh ? GrassMesh : Cube, GrassMesh ? nullptr : Grass, false, 12500);
    auto* BrownBlades = GrassMesh ? GreenBlades : Batch(Cube, DryGrass, false, 12500);
    const FVector BladeSize=GrassMesh ? GrassMesh->GetBounds().BoxExtent*2 : FVector(100);
    for (int I=0; I<1200; ++I)
    {
        // Concentrate coverage in the visible verge instead of diluting it over
        // the full terrain. Irregular band depth leaves patches of natural soil.
        FVector P;
        if (I<480) P=FVector(Scenery.FRandRange(4210,5900),Scenery.FRandRange(-3000,6000),-23);
        else if (I<880) P=FVector(Scenery.FRandRange(-2800,4150),Scenery.FRandRange(3500,4900),-23);
        else if (I<1020) P=FVector(Scenery.FRandRange(-4200,-2910),Scenery.FRandRange(-3450,3500),-23);
        else P=FVector(Scenery.FRandRange(-2800,4110),Scenery.FRandRange(-4610,-3510),-23);
        auto* B=I%3 ? GreenBlades : BrownBlades;
        const int Count=GrassMesh ? 1 : 7;
        for (int J=0; J<Count; ++J)
        {
            const float Height=Scenery.FRandRange(38,90);
            FVector Q=P+FVector(Scenery.FRandRange(-26,26),Scenery.FRandRange(-26,26),GrassMesh?0:Height*.4f);
            const float Spread=Scenery.FRandRange(125,205);
            const FVector Size=GrassMesh ? FVector(Spread,Spread,Height) : FVector(3,1.5f,Height);
            const FVector Scale(Size.X/FMath::Max(1.0,BladeSize.X),Size.Y/FMath::Max(1.0,BladeSize.Y),Size.Z/FMath::Max(1.0,BladeSize.Z));
            B->AddInstance(FTransform(FRotator(Scenery.FRandRange(-5,5),Scenery.FRandRange(0,360),Scenery.FRandRange(-5,5)),Q,Scale),true);
        }
    }
}

#include "../Source/Clanka/Simulation.h"
#include <iostream>
#include <stdexcept>
using namespace splash;
static int checks=0;
void require(bool ok,const char* why) { ++checks; if(!ok) throw std::runtime_error(why); }
void advance(Simulation& s,int ticks) { for(int i=0;i<ticks;++i) s.tick(); }
// Finish an already soaked balloon to inspect scoring at a deterministic boundary.
void finishBalloon(Simulation& s,BalloonKind kind=BalloonKind::Normal) {
    s.running=true; s.automatic=false; s.verified=true; s.selected=0;
    auto& target=s.balloons[0];
    target.alive=true; target.protectedZone=false; target.confidence=1;
    target.kind=kind; target.p={1000,0,400}; target.wet=.999;
    s.aim=(target.p-s.nozzle).unit(); s.tick();
}
int main() {
    try {
        Simulation a,b; b.continuous=false;
        advance(a,240); require(a.time==0,"Ready state must not advance");
        a.action(Action::Run); b.action(Action::Run);
        advance(a,1800); advance(b,1800);
        require(a.popped>0,"Authorized demo must splash balloons");
        require(a.popped==b.popped && a.tank==b.tank && a.score==b.score && a.popSerial==b.popSerial,"Identical inputs must produce identical state");
        a.action(Action::Stop); auto tank=a.tank; advance(a,240);
        require(!a.spraying && a.tank==tank && a.drops.empty(),"Stop must disable water");
        a.action(Action::Run); advance(a,240); require(!a.verified,"Run must not bypass stop latch");
        a.action(Action::ReleaseStop); advance(a,240); require(a.tank==tank,"Release must not reauthorize water");
        a.action(Action::Verify); advance(a,120); a.action(Action::Pause); double now=a.time;
        advance(a,240); require(a.time==now && !a.spraying,"Pause must freeze simulation");
        a.action(Action::Pause); a.action(Action::Refill); advance(a,361);
        require(a.tank>99,"Refill must restore tank");
        a.action(Action::Next); require(!a.automatic && !a.verified,"Manual selection requires verification");
        a.action(Action::Verify); a.balloons[a.selected].p.y=800; advance(a,1);
        require(!a.spraying,"Protected zone must inhibit spraying");
        Simulation empty; empty.action(Action::Run); empty.autoRefill=false; empty.tank=0; advance(empty,400);
        require(!empty.spraying && empty.popped==0,"Empty tank must not splash");
        advance(b,24000); require(b.complete && !b.spraying && b.time==180,"Session must end at 180 seconds");
        require(b.score>0 && b.wave>1 && b.popEvents.size()>8,"Automatic demo must refill and advance through multiple waves");
        b.action(Action::Reset); require(b.time==0 && b.popped==0 && !b.verified && b.score==0 && b.combo==0 && b.wave==1 && b.popEvents.empty(),"Reset must restore safe initial state and clear game history");
        Simulation scoring;
        for(int i=0;i<4;++i) finishBalloon(scoring);
        require(scoring.score==500 && scoring.combo==4 && scoring.multiplier==2 && scoring.bestCombo==4,"A four-pop streak must earn a 2x multiplier");
        for(int i=0;i<4;++i) finishBalloon(scoring);
        require(scoring.wave==2 && scoring.waveProgress==0 && scoring.score==2000,"Eight pops must clear a wave and award its 500-point bonus");
        require(scoring.popEvents.back().wave==1 && scoring.popEvents.back().points==300 && scoring.popEvents.back().serial==8,"Pop records must preserve awarded points and the originating wave");
        const auto recordedPosition=scoring.popEvents.front().p;
        scoring.balloons[0].p={0,0,0};
        require(scoring.popEvents.front().p.x==recordedPosition.x,"Pop positions must survive balloon movement");
        scoring.verified=false; scoring.action(Action::Pause); const double comboTime=scoring.comboRemaining;
        advance(scoring,1000); require(scoring.comboRemaining==comboTime,"Paused play must preserve the combo window");
        scoring.action(Action::Pause); advance(scoring,841);
        require(scoring.combo==0 && scoring.multiplier==1 && scoring.bestCombo==8,"Expired combos must reset the multiplier and preserve the best streak");
        Simulation bonus; finishBalloon(bonus,BalloonKind::Bonus);
        require(bonus.score==300 && bonus.popEvents[0].kind==BalloonKind::Bonus,"Gold balloons must award 300 points and report their kind");
        Simulation tough; finishBalloon(tough,BalloonKind::Tough);
        require(tough.score==200,"Tough balloons must award 200 points");
        Simulation normalSoak,toughSoak;
        for(auto* s:{&normalSoak,&toughSoak}) {
            s->action(Action::Run); s->automatic=false; s->selected=0;
            s->balloons[0].p={1000,0,400}; s->balloons[0].confidence=1;
            s->aim=(s->balloons[0].p-s->nozzle).unit();
        }
        toughSoak.balloons[0].kind=BalloonKind::Tough;
        normalSoak.tick(); toughSoak.tick();
        require(toughSoak.balloons[0].wet<normalSoak.balloons[0].wet,"Tough balloons must require more water exposure");
        normalSoak.tick();
        require(!normalSoak.drops.empty() && (normalSoak.drops.front().p-normalSoak.nozzle).length()>220,"Visible water must emerge beyond the barrel tip");
        require(normalSoak.drops.front().life>0 && normalSoak.drops.front().life<.5,"Nearby balloons must shorten the cosmetic water trail");
        normalSoak.verified=false; advance(normalSoak,70);
        require(normalSoak.drops.empty(),"Disengaged water trails must expire near their original target");
        Simulation refill; refill.action(Action::Run); refill.tank=0; refill.tick();
        require(refill.refill>0 && refill.tank==0 && !refill.spraying,"Empty tanks must begin an automatic refill without spraying");
        advance(refill,361); require(refill.tank>99,"Automatic refill must restore the tank");
        Simulation disarmed; disarmed.running=true; disarmed.tank=0; advance(disarmed,500);
        require(disarmed.refill==0 && disarmed.tank==0,"Automatic refill must require an authorized session");
        Simulation system;
        system.action(Action::Engage); require(!system.running && !system.verified,"Engage must require power on");
        system.action(Action::Verify); require(!system.running && !system.verified,"Legacy verify must not power on or arm an offline system");
        system.action(Action::PowerOn); advance(system,600);
        require(system.running && system.automatic && system.target()!=nullptr && !system.verified && !system.spraying && system.tank==100 && system.popped==0,"Power on must acquire tracks without virtual water");
        system.action(Action::Engage); advance(system,1200);
        require(system.verified && system.popped>0,"Engage must explicitly authorize autonomous virtual water");
        system.action(Action::Engage); require(system.verified,"Repeated engage must not disarm the system");
        system.action(Action::PowerOff); const double poweredOffTime=system.time;
        advance(system,600);
        require(!system.running && !system.verified && !system.spraying && system.drops.empty() && system.time==poweredOffTime,"Power off must disarm and stop the simulation");
        system.action(Action::PowerOn); require(!system.verified,"Power cycling must require engage again");
        system.action(Action::Engage); advance(system,24000);
        require(system.continuous && system.time>180 && !system.complete && system.verified,"Continuous operation must remain active beyond 180 seconds");
        system.action(Action::Stop); system.action(Action::Engage);
        require(!system.verified,"Engage must respect the stop latch");
        Simulation ring;
        for(int i=0;i<140;++i) finishBalloon(ring);
        require(ring.popEvents.size()==128 && ring.popEvents.front().serial==13 && ring.popEvents.back().serial==140,"Event retention must stay bounded while preserving monotonic serials");
        std::cout<<"PASS: "<<checks<<" simulation checks\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<"\n"; return 1; }
}

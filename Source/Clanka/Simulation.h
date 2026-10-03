#pragma once
// Engine-independent, deterministic game logic. Distances are fictional game units.
// Water is an arcade exposure model, not a hydraulic or physical aiming model.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace splash {
struct Vec {
    double x=0,y=0,z=0;
    Vec operator+(Vec b) const { return {x+b.x,y+b.y,z+b.z}; }
    Vec operator-(Vec b) const { return {x-b.x,y-b.y,z-b.z}; }
    Vec operator*(double s) const { return {x*s,y*s,z*s}; }
    double dot(Vec b) const { return x*b.x+y*b.y+z*b.z; }
    double length() const { return std::sqrt(dot(*this)); }
    Vec unit() const { double n=length(); return n>1e-9 ? *this*(1/n) : Vec{1,0,0}; }
};
enum class BalloonKind { Normal, Bonus, Tough };
struct Balloon {
    int id=0; Vec p; double phase=0, wet=0, confidence=0, respawn=0;
    bool alive=true, protectedZone=false;
    BalloonKind kind=BalloonKind::Normal;
};
struct Drop { Vec p,v; double life=0; };
struct Event { double time; std::string message; };
// Renderers consume serials once; positions survive balloon respawn and event pruning.
struct PopEvent {
    uint32_t serial=0; double time=0; Vec p; int balloonId=0;
    BalloonKind kind=BalloonKind::Normal;
    int points=0, combo=0, multiplier=1, wave=1;
};
enum class Action { Run, Pause, Stop, ReleaseStop, Next, Verify, ToggleAuto, Refill, Reset, Calm, Breeze, Gusts, ToggleAutoRefill, PowerOn, Engage, PowerOff };

class Simulation {
    uint32_t randomState=4107;
    double sprayAccumulator=0;
    uint32_t random() { randomState=1664525u*randomState+1013904223u; return randomState; }
    double unitRandom() { return (random()>>8)/16777216.0; }
    void place(Balloon& b) {
        b.phase=unitRandom()*6.283185307;
        b.p={850+unitRandom()*1350,-900+unitRandom()*1500,350+unitRandom()*650};
        b.wet=0; b.confidence=0; b.respawn=0; b.alive=true;
        b.protectedZone=b.p.y>620;
        // Keep the mix deterministic without consuming extra random values.
        const int variety=(b.id+wave-1)%5;
        b.kind=variety==0 ? BalloonKind::Bonus : variety==3 ? BalloonKind::Tough : BalloonKind::Normal;
    }
    void log(const std::string& s) {
        events.push_back({time,s});
        if(events.size()>512) events.erase(events.begin());
    }
public:
    static constexpr double Step=1.0/120.0;
    Vec nozzle{0,0,170}, aim{1,0,0.35};
    std::vector<Balloon> balloons;
    std::vector<Drop> drops;
    std::vector<Event> events;
    std::vector<PopEvent> popEvents;
    double time=0, tank=100, refill=0, sprayingTime=0;
    int selected=-1, popped=0, escaped=0, scenario=1, wave=1;
    int score=0, combo=0, bestCombo=0, multiplier=1, waveProgress=0;
    static constexpr int waveTarget=8;
    double comboRemaining=0;
    uint32_t popSerial=0;
    bool autoRefill=true;
    bool continuous=true;
    bool running=false, paused=false, stopped=false, automatic=true, verified=false, spraying=false, complete=false;
    Simulation() { reset(); }
    void reset() {
        randomState=4107; time=0; tank=100; refill=0; popped=escaped=0; wave=1;
        selected=-1; running=paused=stopped=verified=spraying=complete=false; automatic=true;
        scenario=1; score=combo=bestCombo=waveProgress=0; multiplier=1;
        comboRemaining=0; popSerial=0; autoRefill=true; continuous=true;
        aim=Vec{1,0,.35}.unit(); sprayAccumulator=0; sprayingTime=0;
        balloons.clear(); drops.clear(); events.clear(); popEvents.clear();
        for(int i=0;i<10;++i) { Balloon b; b.id=i+1; place(b); balloons.push_back(b); }
        log("Range ready. POWER ON, then ENGAGE to begin.");
    }
    void action(Action a) {
        switch(a) {
        case Action::PowerOn:
            if(!stopped && !complete && !running) {
                running=true; paused=false; automatic=true; verified=false;
                log("System on. Automatic tracking active. ENGAGE to authorize virtual water.");
            }
            break;
        case Action::Engage:
            if(running && !stopped && !complete && !verified) {
                verified=true; log("Virtual water engaged.");
            }
            break;
        case Action::PowerOff:
            running=false; paused=false; verified=false; spraying=false;
            refill=0; sprayAccumulator=0; drops.clear();
            log("System off. Virtual water disabled.");
            break;
        case Action::Run:
            if(!stopped && !complete) { running=true; paused=false; verified=true; log("Operator authorized virtual water."); } break;
        case Action::Pause: if(running && !complete) { paused=!paused; spraying=false; log(paused?"Simulation paused.":"Simulation resumed."); } break;
        case Action::Stop: stopped=true; verified=false; spraying=false; drops.clear(); log("STOP latched. Water disabled."); break;
        case Action::ReleaseStop: stopped=false; verified=false; log("Stop released. ENGAGE to authorize virtual water."); break;
        case Action::ToggleAuto: automatic=!automatic; selected=-1; verified=false; log(automatic?"Automatic selection. ENGAGE to spray.":"Manual selection. TAB then ENGAGE."); break;
        case Action::Verify: if(running && !stopped && !complete) { verified=!verified; log(verified?"Virtual water authorized.":"Virtual water disarmed."); } break;
        case Action::Next:
            for(size_t n=0;n<balloons.size();++n) {
                selected=(selected+1)%static_cast<int>(balloons.size());
                if(balloons[selected].alive && !balloons[selected].protectedZone) break;
            }
            automatic=false; verified=false; log("Track selected. ENGAGE to spray."); break;
        case Action::Refill: if(refill<=0 && tank<99.9) { refill=3; spraying=false; log("Refilling: three seconds."); } break;
        case Action::Reset: reset(); break;
        case Action::Calm: scenario=0; log("Scenario: calm."); break;
        case Action::Breeze: scenario=1; log("Scenario: breeze."); break;
        case Action::Gusts: scenario=2; log("Scenario: gusts."); break;
        case Action::ToggleAutoRefill: autoRefill=!autoRefill; log(autoRefill?"Automatic refill enabled.":"Automatic refill disabled."); break;
        }
    }
    const Balloon* target() const { return selected>=0 && selected<static_cast<int>(balloons.size()) ? &balloons[selected] : nullptr; }
    bool eligible(const Balloon& b) const { return b.alive && !b.protectedZone && b.confidence>=0.75; }
    const char* status() const {
        if(stopped) return "STOP LATCHED";
        if(complete) return "SESSION COMPLETE";
        if(paused) return "PAUSED";
        if(!running) return "POWER OFF";
        if(refill>0) return "REFILLING";
        if(tank<=0) return "TANK EMPTY";
        if(!verified) return "AWAITING ENGAGE";
        if(spraying) return "SPLASHING";
        return "ACQUIRING TRACK";
    }
    void tick() {
        spraying=false;
        if(!running || paused || complete) return;
        time+=Step;
        if(!continuous && time>=180) { time=180; complete=true; verified=false; drops.clear(); log("Session complete. Reset to play again."); return; }
        comboRemaining=std::max(0.0,comboRemaining-Step);
        if(comboRemaining==0) { combo=0; multiplier=1; }
        if(autoRefill && !stopped && verified && tank<=0 && refill<=0) {
            refill=3; log("Automatic refill: back in three seconds.");
        }
        if(refill>0) { refill=std::max(0.0,refill-Step); if(refill==0) { tank=100; log("Tank refilled."); } }
        for(auto& b:balloons) {
            if(!b.alive) { b.respawn-=Step; if(b.respawn<=0) place(b); continue; }
            double speed=(40+scenario*45)*(1+std::min(6,wave-1)*.09);
            if(b.kind==BalloonKind::Bonus) speed*=1.16;
            if(b.kind==BalloonKind::Tough) speed*=.8;
            b.p.y+=Step*(speed+std::sin(time*1.7+b.phase)*scenario*35);
            b.p.z+=Step*std::sin(time*.8+b.phase)*(18+scenario*15);
            b.protectedZone=b.p.y>620;
            b.confidence=b.protectedZone ? 0 : std::min(1.0,b.confidence+Step*.7);
            b.wet=std::max(0.0,b.wet-Step*.055);
            if(b.p.y>1150) { ++escaped; place(b); b.p.y=-1100; }
        }
        if(automatic && (selected<0 || !eligible(balloons[selected]))) {
            selected=-1; double best=1e20;
            for(size_t i=0;i<balloons.size();++i) {
                auto& b=balloons[i]; double cost=(b.p-nozzle).length()-b.wet*500;
                if(eligible(b) && cost<best) { best=cost; selected=static_cast<int>(i); }
            }
        }
        const Balloon* t=target();
        if(!stopped && t && eligible(*t)) {
            Vec desired=(t->p-nozzle).unit();
            aim=(aim+(desired-aim)*std::min(1.0,Step*4)).unit();
            if(verified && tank>0 && refill==0 && aim.dot(desired)>.999) {
                spraying=true; sprayingTime+=Step; tank=std::max(0.0,tank-Step*7);
                auto& hit=balloons[selected];
                const double soak=hit.kind==BalloonKind::Tough ? .58 : hit.kind==BalloonKind::Bonus ? 1.35 : .95;
                hit.wet+=Step*soak;
                if(hit.wet>=1) {
                    hit.wet=1; hit.alive=false; hit.respawn=std::max(1.2,3-(wave-1)*.15); ++popped;
                    ++combo; bestCombo=std::max(bestCombo,combo); comboRemaining=7;
                    multiplier=1+std::min(4,(combo-1)/3);
                    const int base=hit.kind==BalloonKind::Bonus ? 300 : hit.kind==BalloonKind::Tough ? 200 : 100;
                    const int points=base*multiplier; score+=points;
                    popEvents.push_back({++popSerial,time,hit.p,hit.id,hit.kind,points,combo,multiplier,wave});
                    if(popEvents.size()>128) popEvents.erase(popEvents.begin());
                    log("Balloon "+std::to_string(hit.id)+" splashed! +"+std::to_string(points)+" ("+std::to_string(multiplier)+"x)");
                    if(++waveProgress>=waveTarget) {
                        waveProgress=0; ++wave; score+=500;
                        log("Wave "+std::to_string(wave-1)+" clear! +500. Wave "+std::to_string(wave)+" incoming.");
                    }
                    if(!automatic) verified=false;
                }
            }
        }
        if(spraying) {
            sprayAccumulator+=Step*70;
            // Cosmetic particles begin at the visible barrel tip and fade near
            // the balloon. They do not participate in the arcade exposure model.
            const Vec streamStart=nozzle+aim*220;
            const double streamLife=std::max(.1,std::min(1.2,t ? (t->p-streamStart).length()/2300.0 : 1.0));
            while(sprayAccumulator>=1) {
                sprayAccumulator-=1;
                Vec jitter{(unitRandom()-.5)*.03,(unitRandom()-.5)*.03,(unitRandom()-.5)*.03};
                drops.push_back({streamStart,(aim+jitter)*2300,streamLife});
            }
        } else sprayAccumulator=0;
        for(auto& d:drops) { d.p=d.p+d.v*Step; d.v.z-=40*Step; d.life-=Step; }
        drops.erase(std::remove_if(drops.begin(),drops.end(),[](const Drop& d){ return d.life<=0 || d.p.z<5; }),drops.end());
    }
};
}

#pragma once
#include "TraversalAudio.h"
#include "RuntimeVersion.h"
#include "SoundFiles.h"

namespace fc {

class TraversalAudioRuntime {
public:
    struct GroupStats {
        unsigned queued{},rejected{},sampled{},playing{},durationKnown{},unobserved{},cancelled{};
    };
private:
    struct Voice {
        RE::BSSoundHandle sound{};
        float age{};
        unsigned group{},probe{};
        std::uint64_t duration{};
        bool pending{};
    };
    std::array<RE::BGSSoundDescriptorForm*,4> descriptors{};
    /// Groups without an ESP descriptor; they play bundled wav files directly.
    std::array<bool,4> fileMode{};
    std::array<unsigned,4> lastFile{};
    unsigned picks{};
    std::array<Voice,6> voices{};
    std::array<GroupStats,4> stats{};
    TraversalAudio timing;
    unsigned nextVoice{};
    bool installed{},dirty{};
    static unsigned slot(SoundCue cue) {
        switch(cue){case SoundCue::step:return 0;case SoundCue::grip:return 1;
            case SoundCue::push:return 2;case SoundCue::top:return 3;}
        return 0;
    }
    static const char* name(unsigned group) {
        constexpr std::array names{"step","grip","push","top"};return names[group];
    }
    static bool position(RE::BSSoundHandle& sound,Vec point) {
        if(!runtime::isSE()) {
            auto* manager=RE::BSAudioManager::GetSingleton();
            if(!manager||sound.soundID==RE::BSSoundHandle::kInvalidID)return false;
            manager->ComposeMessage(RE::SOUND_MSG::SetPosition,sound.soundID,0,nullptr,{},RE::NiPoint3{point.x,point.y,point.z});
            return true;
        }
        using Function=bool(*)(RE::BSSoundHandle*,float,float,float);
        REL::Relocation<Function> function{REL::ID(66370)};
        return function(&sound,point.x,point.y,point.z);
    }
    void summarize() {
        if(!dirty)return;
        for(unsigned i=0;i<stats.size();++i) {
            const auto& s=stats[i];if(!s.queued&&!s.rejected)continue;
            SKSE::log::info("Traversal audio totals: group={}, queued={}, rejected={}, sampled={}, nativePlaying={}, durationOnly={}, unobserved={}, cancelled={}",
                name(i),s.queued,s.rejected,s.sampled,s.playing,s.durationKnown,s.unobserved,s.cancelled);
        }
        dirty=false;
    }
    void cancelProbe(Voice& voice) {
        if(voice.pending){++stats[voice.group].cancelled;voice.pending=false;dirty=true;}
    }
    void reject(unsigned group,const char* stage) {
        auto& s=stats[group];++s.rejected;dirty=true;
        if(s.rejected<=3||s.rejected%32==0)
            SKSE::log::warn("Traversal sound request rejected: group={}, stage={}, rejected={}",name(group),stage,s.rejected);
    }
    /// Find and validate the ESP sound descriptor of one group.
    ///
    /// # Returns
    /// `nullptr` when the ESP is not loaded or the record is not a usable standard sound.
    static RE::BGSSoundDescriptorForm* lookup(RE::TESDataHandler* data,unsigned group) {
        auto* form=data?data->LookupForm<RE::BGSSoundDescriptorForm>(0x801+group,"FreeClimb.esp"):nullptr;
        const auto* definition=form?form->soundDescriptor:nullptr;
        if(!definition||definition->GetType()!=0x1EEF540A||!definition->category)return nullptr;
        const auto* standard=static_cast<const RE::BGSStandardSoundDef*>(definition);
        const bool valid=standard->outputModel&&standard->category->GetFormType()==RE::FormType::SoundCategory&&
            standard->outputModel->GetFormType()==RE::FormType::SoundOutputModel&&!standard->soundFiles.empty();
        if(!valid)return nullptr;
        SKSE::log::info("Traversal sound descriptor: group={}, form={:08X}, files={}, category={:08X}, output={:08X}",
            name(group),form->GetFormID(),standard->soundFiles.size(),standard->category->GetFormID(),standard->outputModel->GetFormID());
        return form;
    }
    /// Get a sound handle from the ESP descriptor, or from a bundled wav file without the ESP.
    bool acquire(RE::BSAudioManager& manager,RE::BSSoundHandle& sound,unsigned group) {
        if(!fileMode[group])return manager.GetSoundHandle(sound,descriptors[group],0x10);
        const char* file=sounds::pick(group,lastFile[group],++picks);
        if(!file)return false;
        RE::BSResource::ID id;id.GenerateFromPath(file);
        manager.GetSoundHandleByFile(sound,id,0x10,sounds::priority);
        return sound.soundID!=RE::BSSoundHandle::kInvalidID;
    }
    void play(TraversalSound cue,Vec point) {
        const auto group=slot(cue.cue);
        auto* manager=RE::BSAudioManager::GetSingleton();
        if(!manager){reject(group,"manager");return;}
        auto& voice=voices[nextVoice++%voices.size()];
        cancelProbe(voice);
        if(voice.sound.soundID!=RE::BSSoundHandle::kInvalidID)voice.sound.Stop();
        voice={};voice.group=group;

        const char* failed=fileMode[group]?"file":"descriptor";
        bool accepted=acquire(*manager,voice.sound,group);
        const float gain=std::clamp(volume*cue.gain,0.f,1.f);
        if(accepted){failed="position";accepted=position(voice.sound,point);}
        if(accepted){failed="volume";accepted=voice.sound.SetVolume(gain);}
        if(accepted){failed="play";accepted=voice.sound.Play();}
        if(!accepted) {
            if(voice.sound.soundID!=RE::BSSoundHandle::kInvalidID)voice.sound.Stop();
            reject(group,failed);return;
        }
        auto& s=stats[group];++s.queued;dirty=true;

        voice.pending=s.queued<=8||s.queued%32==0;
        if(voice.pending)++s.sampled;
        if(s.queued==1||s.queued%32==0)
            SKSE::log::info("Traversal sound queued: group={}, queued={}, handle={}, gain={:.3f}, category=Footsteps",name(group),s.queued,voice.sound.soundID,gain);
    }
public:
    bool enabled=true;
    float volume=.75f;
    const auto& statistics() const{return stats;}
    /// Resolve sound descriptors. Groups missing from `FreeClimb.esp` fall back to bundled wav files.
    ///
    /// # Returns
    /// `false` only on unsupported runtimes. The ESP is optional.
    bool install() {
        if(!runtime::supported())return false;
        auto* data=RE::TESDataHandler::GetSingleton();
        unsigned missing{};
        for(unsigned i=0;i<descriptors.size();++i) {
            descriptors[i]=lookup(data,i);
            fileMode[i]=descriptors[i]==nullptr;
            if(fileMode[i])++missing;
        }
        installed=true;
        SKSE::log::info("Bundled traversal audio: descriptors={}, files={}, enabled={}, volume={:.2f}",
            unsigned(descriptors.size())-missing,missing,enabled,volume);
        if(missing)SKSE::log::info("FreeClimb.esp sound descriptors missing; playing bundled wav files directly");
        return installed;
    }

    void observe(float dt) {
        if(!std::isfinite(dt)||dt<=0)return;
        constexpr std::array times{.012f,.035f,.070f,.150f,.350f,.750f};
        for(auto& voice:voices) {
            if(!voice.pending)continue;
            voice.age+=std::min(dt,.1f);
            if(voice.age<times[voice.probe])continue;

            auto observed=voice.sound;observed.assumeSuccess=false;
            const bool playing=observed.IsPlaying();
            voice.duration=std::max(voice.duration,observed.GetDuration());
            ++voice.probe;
            if(playing) {
                auto& s=stats[voice.group];++s.playing;voice.pending=false;dirty=true;
                if(s.playing==1||s.playing%32==0)
                    SKSE::log::info("Traversal native audio active: group={}, handle={}, duration={}, delay={:.3f}s (not an audibility measurement)",name(voice.group),voice.sound.soundID,voice.duration,voice.age);
            } else if(voice.probe==times.size()) {
                auto& s=stats[voice.group];voice.pending=false;dirty=true;
                if(voice.duration)++s.durationKnown;else ++s.unobserved;

                if(s.unobserved+s.durationKnown<=3||(s.unobserved+s.durationKnown)%32==0)
                    SKSE::log::warn("Traversal audio observation inconclusive: group={}, handle={}, duration={}, probes={}; no native playing state sampled",name(voice.group),voice.sound.soundID,voice.duration,voice.probe);
            }
        }
    }
    void resetTiming(){timing.reset();summarize();}
    void stop() {
        timing.reset();
        for(auto& voice:voices) {
            cancelProbe(voice);
            if(voice.sound.soundID!=RE::BSSoundHandle::kInvalidID)voice.sound.Stop();
            voice={};
        }
        summarize();
    }
    void update(const Library& library,const Traversal& traversal,const Result& result,float phase,float dt,bool outputReady) {
        if(!enabled||!installed||volume<=0){timing.reset();return;}
        const auto cues=timing.update(result.motion,phase,traversal.position,
            library.contactWeights(result.motion,phase),dt,outputReady,result.completed);
        for(unsigned i=0;i<cues.count;++i) {
            const auto cue=cues.items[i];
            const float height=cue.cue==SoundCue::grip?traversal.cfg.grip:cue.cue==SoundCue::push?30.f:12.f;
            play(cue,traversal.position+Vec{0,0,height});
        }
    }
};
}

#include "traversal/Core.h"
#include <cmath>
#include <string_view>
#include <type_traits>
#include "runtime/RuntimePolicy.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>
namespace mock {
unsigned stage{},nextId{},plays{},stops{},queries{},durationQueries{};
unsigned positionMessages{},positionId{};
std::uint32_t runtimeVersion=fc::runtime::pack(1,5,97);
std::array<float,3> position{};
bool manager=true,playing=false,assumedShortcut=false,vanilla=true;
unsigned fileHandles{},outputModels{};
float categoryVolume=1.f,lastGain{};
std::string lastFile;
const char* lastPlugin{};
std::uint64_t duration=0;
std::vector<std::string> messages;
}
namespace SKSE::log {
template<class...Args>void info(const char* message,Args...){mock::messages.emplace_back(message);}
template<class...Args>void warn(const char* message,Args...){mock::messages.emplace_back(message);}
}
namespace RE {
struct NiPoint3 {float x{},y{},z{};};
enum class SOUND_MSG {SetPosition=19};
struct BSISoundOutputModel {};
struct BGSSoundOutput:BSISoundOutputModel {};
struct BGSSoundCategory {float GetCategoryVolume() const{return mock::categoryVolume;}};
struct BSSoundHandle {
    static constexpr std::uint32_t kInvalidID=0xFFFFFFFF;
    std::uint32_t soundID=kInvalidID;bool assumeSuccess{};
    bool Stop(){++mock::stops;return true;}
    bool SetVolume(float volume){if(volume<0||volume>1)throw std::runtime_error("unbounded gain");mock::lastGain=volume;return mock::stage!=3;}
    void SetOutputModel(const BSISoundOutputModel* model){if(!model)throw std::runtime_error("missing output model");++mock::outputModels;}
    bool Play(){++mock::plays;return mock::stage!=4;}
    bool IsPlaying()const{++mock::queries;if(assumeSuccess){mock::assumedShortcut=true;return true;}return mock::playing;}
    std::uint64_t GetDuration(){++mock::durationQueries;return mock::duration;}
};
namespace BSResource {
struct ID {std::string path;void GenerateFromPath(const char* value){path=value;}};
}
struct BSAudioManager {
    static BSAudioManager* GetSingleton(){static BSAudioManager value;return mock::manager?&value:nullptr;}
    void GetSoundHandleByFile(BSSoundHandle& sound,const BSResource::ID& file,unsigned flags,unsigned priority) {
        if(flags!=0x10)throw std::runtime_error("spatial flags overwritten");
        if(priority!=128)throw std::runtime_error("priority changed");
        ++mock::fileHandles;mock::lastFile=file.path;
        if(mock::stage==1)return;
        sound.soundID=++mock::nextId;sound.assumeSuccess=true;
    }
    void ComposeMessage(SOUND_MSG type,unsigned id,unsigned unused,void* pointer,NiPoint3 empty,NiPoint3 position) {
        if(type!=SOUND_MSG::SetPosition||!id||unused||pointer||empty.x||empty.y||empty.z)
            throw std::runtime_error("invalid AE position message");
        ++mock::positionMessages;mock::positionId=id;
        mock::position={position.x,position.y,position.z};
    }
};
struct TESDataHandler {
    static TESDataHandler* GetSingleton(){static TESDataHandler value;return &value;}
    template<class T>T* LookupForm(unsigned id,const char* plugin) {
        mock::lastPlugin=plugin;
        if(!mock::vanilla)return nullptr;
        if constexpr(std::is_same_v<T,BGSSoundCategory>) {
            if(id!=0xF5FFC)throw std::runtime_error("wrong sound category");
            static BGSSoundCategory category;return &category;
        } else {
            if(id!=0x428B6)throw std::runtime_error("wrong output model");
            static BGSSoundOutput output;return &output;
        }
    }
};
}
namespace REL {
struct Version {
    std::uint32_t value{};
    Version(unsigned major,unsigned minor,unsigned patch,unsigned build):value(fc::runtime::pack(major,minor,patch,build)){}
    std::uint32_t pack()const{return value;}
    bool operator==(const Version&)const=default;
};
struct Module {
    static Module& get(){static Module value;return value;}
    Version version()const{Version value(0,0,0,0);value.value=mock::runtimeVersion;return value;}
};
struct ID{explicit ID(unsigned id){if(id!=66370)throw std::runtime_error("wrong ABI relocation");}};
template<class Function>struct Relocation {
    explicit Relocation(ID){}
    bool operator()(RE::BSSoundHandle*,float,float,float){
        if(mock::runtimeVersion!=fc::runtime::pack(1,5,97))throw std::runtime_error("SE position ABI used on AE");
        return mock::stage!=2;
    }
};
}
namespace fc {
struct Library {std::array<float,4> contactWeights(Motion,float) const{return {};}};
}
#include "audio/TraversalAudioRuntime.h"
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
void step(fc::TraversalAudioRuntime& audio,fc::Vec point={}) {
    audio.resetTiming();fc::Library library;fc::Traversal traversal;fc::Result result;
    traversal.position=point;
    result.motion=fc::Motion::runUp;
    audio.update(library,traversal,result,.20f,.1f,true);
    traversal.position.x+=3;
    audio.update(library,traversal,result,.40f,.1f,true);
}
void observeAll(fc::TraversalAudioRuntime& audio) {for(unsigned i=0;i<100;++i)audio.observe(.01f);}
int main() {
    try {
        fc::TraversalAudioRuntime audio;require(audio.install(),"descriptors rejected");
        step(audio);require(audio.statistics()[0].queued==1,"step not queued");
        require(mock::queries==0,"synchronous status request");
        observeAll(audio);
        require(audio.statistics()[0].unobserved==1&&audio.statistics()[0].playing==0,"queued falsely treated as playing");
        require(mock::queries==6&&mock::durationQueries==6&&!mock::assumedShortcut,"unbounded polls or assumed state shortcut");
        audio.observe(100);require(mock::queries==6,"polling continued after deadline");
        mock::duration=103;step(audio);observeAll(audio);
        require(audio.statistics()[0].durationKnown==1&&audio.statistics()[0].playing==0,"metadata confused with playback");
        mock::playing=true;step(audio);audio.observe(.02f);
        require(audio.statistics()[0].playing==1,"actual asynchronous playback missing");
        unsigned afterSuccess=mock::queries;observeAll(audio);require(mock::queries==afterSuccess,"polling continued after actual playback");
        mock::playing=false;mock::duration=0;
        step(audio);audio.stop();afterSuccess=mock::queries;observeAll(audio);
        require(audio.statistics()[0].cancelled==1&&mock::queries==afterSuccess,"stop did not cancel polls");
        for(unsigned stage=1;stage<=4;++stage){mock::stage=stage;step(audio);}
        mock::manager=false;step(audio);mock::manager=true;mock::stage=0;
        require(audio.statistics()[0].rejected==5&&audio.statistics()[0].queued==4,"stage failures not separated from queues");
        for(unsigned i=0;i<24;++i){step(audio);observeAll(audio);}
        require(audio.statistics()[0].sampled==8,"steady sound traffic not sampling boundedly");
        for(unsigned i=0;i<4;++i){step(audio);observeAll(audio);}
        require(audio.statistics()[0].sampled==9,"periodic later sample missing");
        audio.enabled=false;step(audio);require(audio.statistics()[0].queued==32,"disabled sound emitted");
        require(mock::positionMessages==0,"SE unexpectedly used AE message path");
        for(const auto version:fc::runtime::supportedVersions) {
            if(version==fc::runtime::pack(1,5,97))continue;
            mock::runtimeVersion=version;fc::TraversalAudioRuntime ae;
            require(ae.install(),"supported AE audio rejected");
            const auto before=mock::positionMessages;
            step(ae,{12,24,36});
            require(ae.statistics()[0].queued==1&&mock::positionMessages==before+1,"AE spatial message not queued");
            require(mock::positionId==mock::nextId&&mock::position==std::array<float,3>{15,24,48},"AE spatial payload changed");
            ae.stop();
        }
        mock::runtimeVersion=fc::runtime::pack(1,5,97);
        {
            fc::TraversalAudioRuntime bare;require(bare.install(),"vanilla forms rejected");
            require(std::string_view(mock::lastPlugin)=="Skyrim.esm","forms must come from Skyrim.esm only");
            const auto files=mock::fileHandles,models=mock::outputModels;step(bare);
            require(bare.statistics()[0].queued==1&&mock::fileHandles==files+1&&mock::outputModels==models+1,"bundled wav not routed through output model");
            require(mock::lastFile.starts_with("Sound\\fx\\FreeClimb\\step"),"wrong bundled wav group");
            const float full=mock::lastGain;mock::categoryVolume=.5f;step(bare);mock::categoryVolume=1.f;
            require(std::abs(mock::lastGain-full*.5f)<1e-5f,"footsteps category volume ignored");
            bare.stop();
        }
        mock::vanilla=false;
        {
            fc::TraversalAudioRuntime missing;require(!missing.install(),"audio accepted without vanilla forms");
        }
        mock::vanilla=true;
        mock::runtimeVersion=fc::runtime::pack(1,7,105);fc::TraversalAudioRuntime unsupported;
        require(!unsupported.install(),"unknown runtime audio accepted");
        std::cout<<"PASS: real runtime queue-vs-native-state separation, six-poll deadlines, duration-only/unknown/cancelled states, all failure stages, first-eight/every-32 sampling and disabled playback\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}

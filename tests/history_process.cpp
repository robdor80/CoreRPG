#include <corerpg/history.hpp>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <chrono>
using namespace corerpg;
void check(bool yes){if(!yes)throw std::runtime_error("history process contract");}
int main(int argc,char** argv) {try {
    check(argc==4);std::string mode=argv[1];std::filesystem::path path=argv[2];auto fault=static_cast<HistoryFault>(std::stoul(argv[3]));
    auto locator=path;locator+=".locator";
    if(mode=="write")path+="-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    else {std::ifstream file(locator);std::string resolved;std::getline(file,resolved);check(bool(file));path=resolved;}
    WorldState w;RandomStreams r{872};Timeline t;HistoryArchive a;
    if(mode=="write") {
        for(unsigned i=0;i<8;++i)check(w.create_entity().is_valid());
        check(w.add_relation({EntityId{3},RelationKind{4},EntityId{7}})==RelationChange::applied);
        check(t.advance(w,r,100).status==TimelineStatus::completed);
        check(HistoryPersistence::migrate(path,HistoryCampaignId{9},w,r,t,a)==HistoryStatus::success);
        {std::ofstream file(locator);file<<path.generic_string()<<'\n';check(bool(file));}
        const std::array ids{HistorySelection{EntityId{3},0,20}};
        auto status=HistoryPersistence::consolidate(a,w,r,t,ids,80,nullptr,{},fault);
        check(status==(fault==HistoryFault::none?HistoryStatus::success:HistoryStatus::injected_failure));
        // Terminate without normal C++ cleanup to exercise OS lock release and a fresh reader.
        std::_Exit(0);
    }
    check(mode=="read");check(HistoryPersistence::recover(path,a,w,r,t)==HistoryStatus::success);
    const bool published=fault==HistoryFault::none || fault==HistoryFault::after_publish;
    check(a.generation()==(published?2U:1U));check(w.entity_count()==8 && t.now()==100);
    check(r.master_seed()==872);check(w.create_entity()==EntityId{9});
    auto page=HistoryPersistence::query(a,{},1);check(page.status==HistoryStatus::success && page.facts.size()==(published?1U:0U));
    if(published)check(page.facts[0].relations==std::vector<EntityRelation>{{EntityId{3},RelationKind{4},EntityId{7}}});
    else check(w.has_relation({EntityId{3},RelationKind{4},EntityId{7}}));
    std::cout<<"generation="<<a.generation()<<" verified in fresh process\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

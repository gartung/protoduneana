////////////////////////////////////////////////////////////////////////
// Class:       TruthAnalyzer
// Plugin Type: analyzer (art v3_05_01)
// File:        TruthAnalyzer_module.cc
//
// Generated at Tue Nov 24 17:23:39 2020 by Jacob Calcutt using cetskelgen
// from cetlib version v3_10_00.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Principal/SubRun.h"
#include "canvas/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"
#include "larcore/Geometry/Geometry.h"
#include "larsim/MCCheater/BackTrackerService.h"
#include "larsim/MCCheater/ParticleInventoryService.h"
#include "lardataobj/Simulation/SimEnergyDeposit.h"

#include "protoduneana/Utilities/ProtoDUNETruthUtils.h"

#include "nusimdata/SimulationBase/MCParticle.h"
#include "nusimdata/SimulationBase/MCTruth.h"

#include <deque>

namespace pionana {
  class TruthAnalyzer;

  std::map<size_t, double> GetEDepByTraj(
      const simb::MCParticle * part, int id,
      const std::vector<sim::SimEnergyDeposit> & dep_vec,
      const sim::ParticleList & plist);
  
  std::map<size_t, std::vector<int>> GetEMDaughterByTraj(
      const simb::MCParticle * part,
      const sim::ParticleList & plist);
}

std::map<size_t, double> pionana::GetEDepByTraj(
    const simb::MCParticle * part, int id,
    const std::vector<sim::SimEnergyDeposit> & dep_vec,
    const sim::ParticleList & plist) {

  const simb::MCTrajectory & traj = part->Trajectory();
  
  //First build up the trajectory points
  std::map<size_t, double> results;
  for (size_t i = 0; i < traj.size() - 1; ++i) {
    results[i] = 0.;
  }

  std::map<size_t, std::vector<int>> daughters_by_traj
      = GetEMDaughterByTraj(part, plist);
  
  //Iterate over the deposits
  for (const sim::SimEnergyDeposit & dep : dep_vec) {
    if (dep.TrackID() == id) {
      for (size_t i = 1; i < traj.size(); ++i) {
        if (traj.Z(i-1) <= dep.MidPointZ() && traj.Z(i) > dep.MidPointZ()) {
          results[i-1] += dep.Energy();
          break;
        }
      }
    }
    else {
      for (auto it = daughters_by_traj.begin();
           it != daughters_by_traj.end(); ++it) {
        for (size_t i = 0; i < it->second.size(); ++i) {
          if (it->second[i] == dep.TrackID()) {
            results[it->first] += dep.Energy();
          }
        }
      }
    }
  }
  return results;
}

std::map<size_t, std::vector<int>> pionana::GetEMDaughterByTraj(
    const simb::MCParticle * part,
    const sim::ParticleList & plist) {

  const simb::MCTrajectory & traj = part->Trajectory();
  std::map<size_t, std::vector<int>> results;

  for (int i = 0; i < part->NumberDaughters(); ++i) {
    int id = part->Daughter(i);
    auto daughter = plist[id];   
    std::string process = daughter->Process();

    if ((process.find("Ioni") == std::string::npos) &&
        (process.find("Brem") == std::string::npos) &&
        (process.find("annihil") == std::string::npos)) {
      continue;
    }

    for (size_t j = 1; j < traj.size(); ++j) {
      if (traj.Z(j-1) < daughter->Position().Z() &&
          daughter->Position().Z() < traj.Z(j)) {
        //results[j-1].push_back(id);
        std::deque<int> downstream; 
        downstream.push_back(id);


        while (!downstream.empty()) {
          int d_id = downstream.front();
          auto d_part = plist[d_id];
          results[j-1].push_back(d_id);
          for (int k = 0; k < d_part->NumberDaughters(); ++k) {
            downstream.push_back(d_part->Daughter(k));
          }
          downstream.pop_front();
        }
        break; 
      }
    }
  }

  return results;
}

class pionana::TruthAnalyzer : public art::EDAnalyzer {
public:
  explicit TruthAnalyzer(fhicl::ParameterSet const& p);
  // The compiler-generated destructor is fine for non-base
  // classes without bare pointers or other resource use.

  // Plugins should not be copied or assigned.
  TruthAnalyzer(TruthAnalyzer const&) = delete;
  TruthAnalyzer(TruthAnalyzer&&) = delete;
  TruthAnalyzer& operator=(TruthAnalyzer const&) = delete;
  TruthAnalyzer& operator=(TruthAnalyzer&&) = delete;

  // Required functions.
  void analyze(art::Event const& e) override;

private:

  // Declare member data here.
  art::InputTag fGeneratorTag;
  int fView;
  art::InputTag fSimEDepTag;

};


pionana::TruthAnalyzer::TruthAnalyzer(fhicl::ParameterSet const& p)
    : EDAnalyzer{p},
      fGeneratorTag(p.get<art::InputTag>("GeneratorTag")),
      fView(p.get<int>("View")),
      fSimEDepTag(p.get<art::InputTag>("SimEDepTag")) {
}

void pionana::TruthAnalyzer::analyze(art::Event const& e)
{
  protoana::ProtoDUNETruthUtils truthUtil;
  art::ServiceHandle<cheat::BackTrackerService> bt_serv;
  art::ServiceHandle< cheat::ParticleInventoryService > pi_serv;
  const sim::ParticleList & plist = pi_serv->ParticleList();
  auto mcTruths = e.getValidHandle<std::vector<simb::MCTruth>>(fGeneratorTag);
  art::ServiceHandle<geo::Geometry> geom;
  const simb::MCParticle* true_beam_particle
      = truthUtil.GetGeantGoodParticle((*mcTruths)[0],e);
  const simb::MCTrajectory & true_beam_trajectory = true_beam_particle->Trajectory();


  double init_KE = 0.;
  //size_t init_pt = 0;
  //Get the mass for the beam particle
  int true_beam_ID = true_beam_particle->TrackId();
  int true_beam_PDG = true_beam_particle->PdgCode();

  //std::vector<int> daughters;
  //std::vector<std::vector<const sim::IDE *>> d_IDEs;
  //std::vector<double> d_edep;
  double total_d_edep = 0.;

  std::deque<int> to_check;
  std::vector<const sim::IDE *> daughter_IDEs;
  for (int i = 0; i < true_beam_particle->NumberDaughters(); ++i) {
    int id = true_beam_particle->Daughter(i);
    auto part = plist[id];
    std::string process = part->Process();
    if ((process.find("Ioni") == std::string::npos) &&
        (process.find("Brem") == std::string::npos) &&
        (process.find("annihil") == std::string::npos)) {
      std::cout << "Skipping " << process << std::endl;
      continue;
    }
    //std::cout << "Adding " << id << " " << part->PdgCode() << std::endl;
    to_check.push_back(id);
  }

  while (!to_check.empty()) {
    const int id = to_check.front();
    to_check.pop_front();

    std::vector<const sim::IDE *> ides
        = bt_serv->TrackIdToSimIDEs_Ps(id, geo::View_t(fView));
    daughter_IDEs.insert(daughter_IDEs.end(), ides.begin(), ides.end());

    for (size_t i = 0; i < ides.size(); ++i) {
      total_d_edep += ides[i]->energy;
    }

    //Get the daughters of this one
    auto part = plist[id];
    std::cout << id << " Has daughters: ";
    for (int i = 0; i < part->NumberDaughters(); ++i) {
      to_check.push_back(part->Daughter(i));
      std::cout << part->Daughter(i) << " ";
      //auto next_part = plist[part->Daughter(i)];
      //std::cout << next_part->Process() << std::endl;
    }
    std::cout << std::endl;
  }

  /*double mass = 139.57;
  if( true_beam_PDG == 2212 ) mass = 938.27;
  else if( abs(true_beam_PDG) == 211 ) mass = 139.57;
  else if( abs(true_beam_PDG) == 11 ) mass = .511;
  else if( abs(true_beam_PDG) == 321 ) mass = 321;
  else if( abs(true_beam_PDG) == 13 )  mass = 105.66;*/

  std::cout << "Getting IDEs from " << fView << std::endl;
  auto view2_IDEs = bt_serv->TrackIdToSimIDEs_Ps( true_beam_ID, geo::View_t(fView) );
  std::sort(view2_IDEs.begin(), view2_IDEs.end(),
            [](const sim::IDE * i1, const sim::IDE * i2) {return (i1->z < i2->z);});

  double ide_z = (view2_IDEs.size() ? view2_IDEs[0]->z : -999.);
  std::cout << "First IDE: " << ide_z << std::endl;

  std::sort(daughter_IDEs.begin(), daughter_IDEs.end(),
            [](const sim::IDE * i1, const sim::IDE * i2) {return (i1->z < i2->z);});
  if (daughter_IDEs.size())
    std::cout << "First daughter IDE: " << daughter_IDEs[0]->z << std::endl;

  auto true_beam_procs = true_beam_trajectory.TrajectoryProcesses();
  std::map<size_t, unsigned char> proc_map(true_beam_procs.begin(),
                                           true_beam_procs.end());


  for (size_t i = 1; i < true_beam_trajectory.size(); ++i) {
    double z0 = true_beam_trajectory.Z(i-1);
    double x0 = true_beam_trajectory.X(i-1);
    double y0 = true_beam_trajectory.Y(i-1);
    geo::Point_t test_point{x0, y0, z0};
    const TGeoMaterial * mat = geom->Material(test_point);
    std::cout << "Point " << i-1 << " (" << x0 << ", " << y0 <<
                 ", " << z0 << ") Material " << mat->GetName() << " " <<
                 (proc_map.find(i-1) != proc_map.end() ?
                  true_beam_trajectory.KeyToProcess(proc_map[i-1]) :
                  "") << " " << 1.e3*true_beam_trajectory.E(i-1) << 
                 std::endl;
    if (view2_IDEs.size()) {
      double z1 = true_beam_trajectory.Z(i);
      double x1 = true_beam_trajectory.X(i);
      double y1 = true_beam_trajectory.Y(i);
      if (z0 < ide_z && z1 > ide_z) {
        geo::Point_t test_point_1{x1, y1, z1};
        const TGeoMaterial * mat1 = geom->Material(test_point_1);
        init_KE = 1.e3 * true_beam_trajectory.E(i-1);
        std::cout << "Found matching position " << z0 << " " << ide_z <<
                     " " << z1 << " Material " << mat1->GetName() << std::endl;
        std::cout << "init KE: " << init_KE << std::endl;
        //init_pt = i - 1;

        std::cout << "energies: " << 1.e3 * true_beam_trajectory.E(i-1) <<
                     " " << 1.e3 * true_beam_trajectory.E(i) << std::endl;
        std::cout << "Second to last, last point -- (Z, E): " << "(" <<
                     true_beam_trajectory.Z(true_beam_trajectory.size()-2) << ", " <<
                     1.e3*true_beam_trajectory.E(true_beam_trajectory.size()-2) << "), (" <<
                     true_beam_trajectory.Z(true_beam_trajectory.size()-1) << ", " <<
                     1.e3*true_beam_trajectory.E(true_beam_trajectory.size()-1) << ")" <<
                     std::endl;
        std::cout << "End process: " << true_beam_particle->EndProcess() << std::endl;
        break;
      }
    }
    std::cout << std::endl;
  }

  for (size_t i = 0; i < true_beam_trajectory.size(); ++i) {
    std::cout << "Z, E: " << true_beam_trajectory.Z(i) << " " <<
                 true_beam_trajectory.E(i) << std::endl;
  }
  if (view2_IDEs.size()) {
    std::cout << std::endl;
    double total_edep = 0.;
    for (size_t i = 0; i < view2_IDEs.size(); ++i) {
      total_edep += view2_IDEs[i]->energy;
      //std::cout << "\t" << " " << view2_IDEs[i]->trackID << " " <<
      //             view2_IDEs[i]->z << " " << view2_IDEs[i]->energy <<
      //             std::endl;
    }

    std::cout << "Total edep: " << total_edep << std::endl;
    std::cout << "From daughters: " << total_d_edep << std::endl;
    std::cout << "Combined: " << total_edep + total_d_edep << std::endl;
    std::cout << "DeltaE: " << init_KE - (1.e3*true_beam_trajectory.E(true_beam_trajectory.size()-2)) << std::endl;
    std::cout << "IDE - traj: " << total_edep + total_d_edep - (init_KE - (1.e3*true_beam_trajectory.E(true_beam_trajectory.size()-2))) << std::endl;
    std::cout << "Energies: " << (1.e3*true_beam_trajectory.E(0)) << " " << init_KE << " " <<
                 (1.e3*true_beam_trajectory.E(true_beam_trajectory.size()-2)) << " " << (1.e3*true_beam_trajectory.E(true_beam_trajectory.size()-1))
                 << std::endl;
    std::cout << "PDG: " << true_beam_PDG << std::endl;

    int n_elast = 0;
    auto true_beam_procs = true_beam_trajectory.TrajectoryProcesses();
    for (auto it = true_beam_procs.begin();
         it != true_beam_procs.end(); ++it) {
      if (true_beam_trajectory.KeyToProcess(it->second) == "hadElastic") {
        ++n_elast;
      }
    }
    std::cout << "N Elastics " << n_elast << std::endl;
  }

  try{
    auto sim_edep_vec
        = e.getValidHandle<std::vector<sim::SimEnergyDeposit>>(fSimEDepTag);
    double total_sim_edep = 0.;
    double dep_min_z = std::numeric_limits<double>::max();
    for (const sim::SimEnergyDeposit & dep : (*sim_edep_vec)) {
      if (dep.TrackID() != true_beam_ID) continue;
      total_sim_edep += dep.Energy();
      if (dep.MidPointZ() < dep_min_z) dep_min_z = dep.StartZ();
    }
    std::cout << "Min dep z: " << dep_min_z << std::endl;

    to_check.clear();
    for (int i = 0; i < true_beam_particle->NumberDaughters(); ++i) {
      int id = true_beam_particle->Daughter(i);
      auto part = plist[id];
      std::string process = part->Process();
      if ((process.find("Ioni") == std::string::npos) &&
          (process.find("Brem") == std::string::npos) &&
          (process.find("annihil") == std::string::npos)) {
        std::cout << "Skipping " << process << std::endl;
        continue;
      }
      to_check.push_back(id);
    }

    while (!to_check.empty()) {
      const int id = to_check.front();
      to_check.pop_front();

      for (const sim::SimEnergyDeposit & dep : (*sim_edep_vec)) {
        if (dep.TrackID() != id) continue;
        total_sim_edep += dep.Energy();
      }

      //Get the daughters of this one
      auto part = plist[id];
      for (int i = 0; i < part->NumberDaughters(); ++i) {
        to_check.push_back(part->Daughter(i));
      }
    }
    std::cout << "Total Sim EDep: " << total_sim_edep << " " << true_beam_PDG << std::endl;

    for (size_t i = 1; i < true_beam_trajectory.size(); ++i) {
      double z0 = true_beam_trajectory.Z(i-1);
      double x0 = true_beam_trajectory.X(i-1);
      double y0 = true_beam_trajectory.Y(i-1);
      geo::Point_t test_point{x0, y0, z0};
      const TGeoMaterial * mat = geom->Material(test_point);
      double z1 = true_beam_trajectory.Z(i);
      std::cout << "checking " << z0 << " " << dep_min_z << " " << z1 << " " <<
                   1.e3*(true_beam_trajectory.E(i-1) -
                         true_beam_trajectory.E(true_beam_trajectory.size()-2)) <<
                   std::endl;
      if (z0 <= dep_min_z && z1 > dep_min_z) {
        init_KE = 1.e3 * true_beam_trajectory.E(i-1);
        std::cout << "Found matching position " << z0 << " " << dep_min_z <<
                     " " << z1 << " " << mat->GetName() << std::endl;
        std::cout << "init KE: " << init_KE << std::endl;
        //init_pt = i - 1;

        std::cout << "energies: " << 1.e3 * true_beam_trajectory.E(i-1) <<
                     " " << 1.e3 * true_beam_trajectory.E(i) << std::endl;
        std::cout << "Second to last, last point -- (Z, E): " << "(" <<
                     true_beam_trajectory.Z(true_beam_trajectory.size()-2) << ", " <<
                     1.e3*true_beam_trajectory.E(true_beam_trajectory.size()-2) << "), (" <<
                     true_beam_trajectory.Z(true_beam_trajectory.size()-1) << ", " <<
                     1.e3*true_beam_trajectory.E(true_beam_trajectory.size()-1) << ")" <<
                     std::endl;
        break;
      }
    }
    std::cout << "SimEDep Delta E: " << init_KE - 1.e3*true_beam_trajectory.E(true_beam_trajectory.size()-2) << std::endl;
    std::cout << "SimEDep Diff: " <<
                 total_sim_edep - (init_KE - 1.e3*true_beam_trajectory.E(true_beam_trajectory.size()-2)) << " " <<
                 (total_sim_edep - (init_KE - 1.e3*true_beam_trajectory.E(true_beam_trajectory.size()-2)))/total_sim_edep << std::endl;

    std::map<size_t, double> edep_by_traj
        = GetEDepByTraj(true_beam_particle, true_beam_ID, (*sim_edep_vec), plist);
    for (auto it = edep_by_traj.begin(); it != edep_by_traj.end(); ++it) {
      if (it->second == 0.) continue;
      std::cout << it->second << " " <<
                   1.e3*(true_beam_trajectory.E(it->first) -
                         true_beam_trajectory.E(it->first + 1)) << std::endl;
    }
  }
  catch(const std::exception & e) {
    std::cout << "can't get sim edep. Moving on" << std::endl;
  }

}

DEFINE_ART_MODULE(pionana::TruthAnalyzer)

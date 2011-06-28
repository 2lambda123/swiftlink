using namespace std;

#include <vector>
#include <cmath>

#include "descent_graph_types.h"
#include "descent_graph_diff.h"
#include "descent_graph.h"
#include "peel_sequence_generator.h"
#include "peeling.h"
#include "pedigree.h"
#include "genetic_map.h"
#include "rfunction.h"
#include "rfunction_builder.h"
#include "sampler_rfunction.h"
#include "locus_sampler.h"
#include "progress.h"
#include "peeler.h"


/*
    this is pretty confusing, perhaps in need of a refactor(?)
    
    TraitRfunction deals with disease traits
    SamplerRfunction deals with phased genotypes
        both disease traits and phased genotypes are represented by "enum phased_trait"
    DescentGraph deals with meiosis indicators
        these are just zeros and ones
        
 */

LocusSampler::LocusSampler(Pedigree* ped, GeneticMap* map) :
    ped(ped), 
    map(map), 
    dg(ped, map),
    rfunctions(),
    peel(ped, map) {
    
    dg.random_descentgraph();
    
    init_rfunctions();
}

LocusSampler::LocusSampler(const LocusSampler& rhs) :
    ped(ped),
    map(map),
    dg(dg),
    rfunctions(),
    peel(rhs.peel) {
    
    copy_rfunctions(rhs);
}

LocusSampler& LocusSampler::operator=(const LocusSampler& rhs) {
    if(this != &rhs) {
		ped = rhs.ped;
		map = rhs.map;
		dg = rhs.dg;
		peel = rhs.peel;
		
		kill_rfunctions();
		copy_rfunctions(rhs);
	}
	        
	return *this;
}

LocusSampler::~LocusSampler() {
    kill_rfunctions();
}

void LocusSampler::init_rfunctions() {
    PeelSequenceGenerator psg(ped);
    psg.build_peel_order();
    
    vector<PeelOperation>& ops = psg.get_peel_order();
    
    RfunctionBuilder<SamplerRfunction> build(ped, map, rfunctions);
    
    for(unsigned i = 0; i < ops.size(); ++i) {
        rfunctions.push_back(build.createRfunction(ops[i]));
    }
}

void LocusSampler::copy_rfunctions(const LocusSampler& rhs) {
    rfunctions.clear();
    for(unsigned i = 0; i < rhs.rfunctions.size(); ++i) {
        SamplerRfunction* rf = new SamplerRfunction(*(rhs.rfunctions[i]));
        rfunctions.push_back(rf);
    }
}

void LocusSampler::kill_rfunctions() {
    for(unsigned i = 0; i < rfunctions.size(); ++i) {
        delete rfunctions[i];
    }
}

unsigned LocusSampler::get_random(unsigned i) {
    return random() % i;
}

unsigned LocusSampler::get_random_locus() {
    return get_random(ped->num_markers());
}

unsigned LocusSampler::sample_hetero_mi(unsigned allele, enum phased_trait trait) {
    if(allele == 0) {
        return (trait == TRAIT_UA) ? 0 : 1;
    }
    else {
        return (trait == TRAIT_UA) ? 1 : 0;
    }
}

unsigned LocusSampler::sample_homo_mi(unsigned personid, unsigned locus, enum parentage parent) {
    // find prob of setting mi to 0
    // find prob of setting mi to 1
    // normalise + sample
    double prob_dist[2];
    
    double prev_theta = exp(map->get_theta(locus-1));
    double next_theta = exp(map->get_theta(locus+1));
    
    prob_dist[0] = ((dg.get(personid, locus-1, parent) == 0) ? 1.0 - prev_theta : prev_theta) \
                 * ((dg.get(personid, locus+1, parent) == 0) ? 1.0 - next_theta : next_theta);
                 
    prob_dist[1] = ((dg.get(personid, locus-1, parent) == 1) ? 1.0 - prev_theta : prev_theta) \
                 * ((dg.get(personid, locus+1, parent) == 1) ? 1.0 - next_theta : next_theta);

    double total = prob_dist[0] + prob_dist[1];
    
    prob_dist[0] /= total;
    prob_dist[1] /= total;
    
    
    return ((random() / static_cast<double>(RAND_MAX)) < prob_dist[0]) ? 0 : 1;
}

// if a parent is heterozygous, then there is one choice of meiosis indicator
// if a parent is homozygous, then sample based on meiosis indicators to immediate left and right    
unsigned LocusSampler::sample_mi(unsigned allele, enum phased_trait trait, \
                                 unsigned personid, unsigned locus, enum parentage parent) {
    switch(trait) {
        case TRAIT_UA:
        case TRAIT_AU:
            return sample_hetero_mi(allele, trait);
        
        case TRAIT_UU:
        case TRAIT_AA:
            return sample_homo_mi(personid, locus, parent);
        
        default:
            abort();
    }
}

void LocusSampler::step() {
    unsigned locus = get_random_locus();
    
    // forward peel
    for(unsigned i = 0; i < rfunctions.size(); ++i) {
        SamplerRfunction* rf = rfunctions[i];
        rf->evaluate(&dg, locus, 0.0);
    }
    
    PeelMatrixKey pmk;
    
    // reverse peel
    for(int i = static_cast<int>(rfunctions.size()) - 1; i > -1; --i) {
        SamplerRfunction* rf = rfunctions[i];
        rf->sample(pmk);
    }
    
    // sample meiosis indicators
    // if a parent is heterozygous, then there is one choice of meiosis indicator
    // if a parent is homozygous, then sample based on meiosis indicators to immediate left and right
    for(unsigned i = 0; i < ped->num_members(); ++i) {
        Person* p = ped->get_by_index(i);
        
        if(p->isfounder()) {
            continue;
        }
        
        enum phased_trait mat_trait = pmk.get(p->get_maternalid());
        enum phased_trait pat_trait = pmk.get(p->get_paternalid());
        
        enum phased_trait trait = pmk.get(i);
        unsigned mat_allele = ((trait == TRAIT_UU) or (trait == TRAIT_UA)) ? 0 : 1;
        unsigned pat_allele = ((trait == TRAIT_UU) or (trait == TRAIT_AU)) ? 0 : 1;
        
        unsigned mat_mi = sample_mi(mat_allele, mat_trait, i, locus, MATERNAL);
        unsigned pat_mi = sample_mi(pat_allele, pat_trait, i, locus, PATERNAL);
        
        // just do a set on the DescentGraph?
        dg.set(i, locus, MATERNAL, mat_mi);
        dg.set(i, locus, PATERNAL, pat_mi);
    }
}

Peeler* LocusSampler::run(unsigned iterations) {
    
    unsigned burnin_steps = iterations * 0.1;
    
    Progress p("Markov Chain:", iterations);
    p.start();
    
    for(unsigned i = 0; i < iterations; ++i) {
        step();

//        if(not dg.likelihood()) {
//            fprintf(stderr, "Illegal DescentGraph state!\n");
//            abort();
//        }
        
        p.increment();
        
        if(iterations < burnin_steps) {
            continue;
        }
            
        if((iterations % _SAMPLE_PERIOD) == 0) {
            peel.process(dg);
        }
    }
    
    p.finish();
    
    return &peel;
}


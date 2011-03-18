#include <cstdio>
#include <cmath>

#include "genetic_map.h"


bool GeneticMap::sanity_check() {
    bool sane = (map.size() == (thetas.size() - 1)) and \
           (map.size() == (inverse_thetas.size() - 1));

    if(not sane) {
        fprintf(stderr, 
            "error in map data: number of markers = %d, number of thetas = %d\n", 
            int(map.size()), int(thetas.size()));
    }
    
    return sane;
}

void GeneticMap::print() {
    printf("GeneticMap: %d loci\n", int(map.size()));
	
	for(unsigned int i = 0; i < map.size(); ++i)
		map[i].print();
		
	printf("\n");
	printf("  thetas:\n");
	for(unsigned int i = 0; i < thetas.size(); ++i)
		printf("\t%f\n", exp(thetas[i]));

	printf("\n");
}

double GeneticMap::haldane(double m) {
    return 0.5 * (1.0 - exp(-2.0 * m));
}

double GeneticMap::get_theta_halfway(unsigned int i) {
    return haldane(get_marker(i+1).get_g_distance() - 
                   get_marker(i).get_g_distance());
}

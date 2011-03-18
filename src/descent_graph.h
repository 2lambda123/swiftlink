#ifndef LKG_DESCENTGRAPH_H_
#define LKG_DESCENTGRAPH_H_

using namespace std;

#include <limits>


enum parentage { 
	MATERNAL,
	PATERNAL
};

const double LOG_ILLEGAL = -numeric_limits<double>::max();

class Pedigree;
class GeneticMap;

class DescentGraph {

 protected:
	char* data;
	Pedigree* ped;
	GeneticMap* map;

 private:
	double prob;
    double marker_transmission; // cache for transmission prob    
    int graph_size; 			// size of descent graph at one loci, 
								// for indexing data
	
	double _transmission_prob();
	double _recombination_prob();
	bool _sum_prior_prob(double* prob);
    bool _best_prior_prob(double* prob);
	bool _genotype_elimination();
	int _offset(unsigned person_id, unsigned locus, enum parentage p);
	int _founder_allele(unsigned person_id, enum parentage p);

 public :
	DescentGraph(Pedigree* ped, GeneticMap* map);
	DescentGraph(const DescentGraph& d);
	virtual ~DescentGraph();

	DescentGraph& operator=(const DescentGraph& d);

	char get(unsigned person_id, unsigned locus, enum parentage p);
	void set(unsigned person_id, unsigned locus, enum parentage p, char value);
	int get_founderallele(unsigned person_id, unsigned loci, enum parentage p);
	bool likelihood(double *prob);
	bool likelihood();
    bool haplotype_likelihood(double *prob);
	bool haplotype_likelihood();
    bool random();
	void print();
    
    void copy_from(DescentGraph& d, unsigned start, unsigned end);
    unsigned data_length();
    void flip_bit(unsigned i);
	void flip_bit(unsigned person_id, unsigned locus, enum parentage p);
    
    char get_bit(unsigned i);
    void set_bit(unsigned i, char b);
    
    bool operator<(const DescentGraph& a) const {
        return prob < a.prob;
    }

	double get_prob() const { return prob; }
	bool illegal() const { return prob == LOG_ILLEGAL; }
};

#endif

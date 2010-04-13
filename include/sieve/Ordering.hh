#ifndef included_ALE_Ordering_hh
#define included_ALE_Ordering_hh

#ifndef  included_ALE_Partitioner_hh
#include <Partitioner.hh>
#endif

PetscErrorCode SPARSEPACKgenrcm(PetscInt *neqns,PetscInt *xadj,PetscInt *adjncy,PetscInt *perm,PetscInt *mask,PetscInt *xls);

namespace ALE {
  template<typename Alloc_ = malloc_allocator<int> >
  class Ordering {
  public:
    typedef Alloc_                   alloc_type;
    typedef IUniformSection<int,int> perm_type;
  public:
    template<typename Mesh>
    static void calculateMeshReordering(const Obj<Mesh>& mesh, Obj<perm_type>& permutation) {
      Obj<perm_type> pointPermutation = new perm_type(permutation->comm(), permutation->debug());
      int *start     = NULL;
      int *adjacency = NULL;
      int *perm      = NULL;
      int  numVertices;

      Partitioner<>::buildDualCSRV(mesh, &numVertices, &start, &adjacency, true);
      std::cout << "  Built dual" << std::endl;
      pointPermutation->setChart(perm_type::chart_type(0, numVertices));
      for(int i = 0; i < numVertices; ++i) pointPermutation->setFiberDimension(i, 1);
      pointPermutation->allocatePoint();
      perm = const_cast<int*>(pointPermutation->restrictSpace());
      int *mask = alloc_type().allocate(numVertices);
      for(int i = 0; i < numVertices; ++i) {alloc_type().construct(mask+i, 1);}
      int *xls  = alloc_type().allocate(numVertices*2);
      for(int i = 0; i < numVertices*2; ++i) {alloc_type().construct(xls+i,  0);}
      // Correct for Fortran numbering
      for(int i = 0; i < start[numVertices]; ++i) ++adjacency[i];
      for(int i = 0; i <= numVertices; ++i) ++start[i];
      PetscErrorCode ierr = SPARSEPACKgenrcm(&numVertices, start, adjacency, perm, mask, xls);CHKERRXX(ierr);
      std::cout << "  Calculated numbering" << std::endl;
      for(int i = 0; i < numVertices; ++i) {alloc_type().destroy(mask+i);}
      alloc_type().deallocate(mask, numVertices);
      for(int i = 0; i < numVertices*2; ++i) {alloc_type().destroy(xls+i);}
      alloc_type().deallocate(xls, numVertices*2);
      // Correct for Fortran numbering
      for(int i = 0; i < numVertices; ++i) --perm[i];
      // Construct closure
      permutation->setChart(mesh->getSieve()->getChart());
      createOrderingClosureV(mesh, pointPermutation, permutation);
     };

    template<typename Mesh, typename Section>
    static void createOrderingClosureV(const Obj<Mesh>& mesh, const Obj<Section>& pointPermutation, const Obj<Section>& permutation, const int height = 0) {
      typedef ISieveVisitor::TransitiveClosureVisitor<typename Mesh::sieve_type> visitor_type;
      const Obj<typename Mesh::sieve_type>& sieve = mesh->getSieve();
      const typename Section::chart_type&   chart = pointPermutation->getChart();
      typename Section::value_type          count = 0;

      PETSc::Log::Event("PermutationClosure").begin();
      for(typename Section::chart_type::const_iterator p_iter = chart.begin(); p_iter != chart.end(); ++p_iter) {
        typename visitor_type::visitor_type nV;
        visitor_type                        cV(*sieve, nV);

	permutation->setFiberDimension(*p_iter, 1);
	sieve->cone(*p_iter, cV);
	if (height) {
	  cV.setIsCone(false);
	  sieve->support(*p_iter, cV);
	}
        permutation->setFiberDimension(*p_iter, 1);
	count = std::max(count, *pointPermutation->restrictPoint(*p_iter));
      }
      std::cout << "  Sized full permutation, count " << count << std::endl;
      permutation->allocatePoint();

      for(typename Section::chart_type::const_iterator p_iter = chart.begin(); p_iter != chart.end(); ++p_iter) {
        typename visitor_type::visitor_type nV;
        visitor_type                        cV(*sieve, nV);

	sieve->cone(*p_iter, cV);
	if (height) {
	  cV.setIsCone(false);
	  sieve->support(*p_iter, cV);
	}

	permutation->updatePoint(*p_iter, pointPermutation->restrictPoint(*p_iter));
	typename std::set<typename Mesh::point_type>::const_iterator begin = cV.getPoints().begin();
	typename std::set<typename Mesh::point_type>::const_iterator end   = cV.getPoints().end();

	++begin; // Skip cell
        for(typename std::set<typename Mesh::point_type>::const_iterator c_iter = begin; c_iter != end; ++c_iter) {
	  permutation->updatePoint(*c_iter, &count);
	  ++count;
        }
      }
      std::cout << "  Constructed full permutation" << std::endl;
      PETSc::Log::Event("PermutationClosure").end();
    };

    template<typename Section, typename Labeling>
    static void relabelSection(Section& section, Labeling& relabeling, Section& newSection) {
      newSection.setChart(section.getChart());

      for(typename Section::point_type p = section.getChart().min(); p < section.getChart().max(); ++p) {
	const typename Section::point_type newP = relabeling.restrictPoint(p)[0];

	newSection.setFiberDimension(newP, section.getFiberDimension(p));
      }
      newSection.allocatePoint();
      for(typename Section::point_type p = section.getChart().min(); p < section.getChart().max(); ++p) {
	const typename Section::point_type newP = relabeling.restrictPoint(p)[0];

	newSection.updatePoint(newP, section.restrictPoint(p));
      }
    };
  };
}
#endif /* included_ALE_Ordering_hh */

#ifndef included_ALE_Distribution_hh
#define included_ALE_Distribution_hh

#ifndef  included_ALE_Mesh_hh
#include <Mesh.hh>
#endif

#ifndef  included_ALE_Partitioner_hh
#include <Partitioner.hh>
#endif

#ifndef  included_ALE_Completion_hh
#include <Completion.hh>
#endif

// Attempt to unify all of the distribution mechanisms:
//   one to many  (distributeMesh)
//   many to one  (unifyMesh)
//   many to many (Numbering)
// as well as things being distributed
//   Section
//   Sieve        (This sends two sections, the points and cones)
//   Numbering    (Should be an integer section)
//   Global Order (should be an integer section with extra methods)
//
// 0) Create the new object to hold the communicated data
//
// 1) Create Overlap
//    There may be special ways to do this based upon what we know at the time
//
// 2) Create send and receive sections over the interface
//    These have a flat topology now, consisting only of the overlap nodes
//    We could make a full topology on the overlap (maybe it is necessary for higher order)
//
// 3) Communication section
//    Create sizer sections on interface (uses constant sizer)
//    Communicate sizes on interface (uses custom filler)
//      Fill send section
//      sendSection->startCommunication();
//      recvSection->startCommunication();
//      sendSection->endCommunication();
//      recvSection->endCommunication();
//
//    Create section on interface (uses previous sizer)
//    Communicate values on interface (uses custom filler)
//      Same stuff as above
//    
// 4) Update new section with old local values (can be done in between the communication?)
//    Loop over patches in new topology
//      Loop over chart from patch in old atlas
//        If this point is in the new sieve from patch
//          Set to old fiber dimension
//    Order and allocate new section
//    Repeat loop, but update values
//
// 5) Update new section with old received values
//    Loop over patches in discrete topology of receive section (these are ranks)
//      Loop over base of discrete sieve (we should transform this to a chart to match above)
//        Get new patch from overlap, or should the receive patches be <rank, patch>?
//        Guaranteed to be in the new sieve from patch (but we could check anyway)
//          Set to recevied fiber dimension
//    Order and allocate new section
//    Repeat loop, but update values
//
// 6) Synchronize PETSc tags (can I get around this?)
namespace ALE {
  template<typename Mesh, typename Partitioner = ALE::Partitioner<> >
  class DistributionNew {
  public:
    typedef typename Mesh::point_type            point_type;
    typedef typename Partitioner::part_type      rank_type;
    typedef ALE::ISection<rank_type, point_type> partition_type;
  public:
    // Given a partition of sieve points, copy the mesh pieces to each process and fuse into the new mesh
    //   Return overlaps for the cone communication
    template<typename Renumbering, typename NewMesh, typename SendOverlap, typename RecvOverlap>
    static void completeMesh(const Obj<Mesh>& mesh, const Obj<partition_type>& partition, Renumbering& renumbering, const Obj<NewMesh>& newMesh, const Obj<SendOverlap>& sendMeshOverlap, const Obj<RecvOverlap>& recvMeshOverlap) {
      typedef ALE::Sifter<rank_type,rank_type,rank_type> part_send_overlap_type;
      typedef ALE::Sifter<rank_type,rank_type,rank_type> part_recv_overlap_type;
      const Obj<part_send_overlap_type> sendOverlap = new part_send_overlap_type(partition->comm());
      const Obj<part_recv_overlap_type> recvOverlap = new part_recv_overlap_type(partition->comm());

      // Create overlap for partition points
      //   TODO: This needs to be generalized for multiple sources
      Partitioner::createDistributionPartOverlap(sendOverlap, recvOverlap);
      // Communicate partition pieces to processes
      Obj<partition_type> overlapPartition = new partition_type(partition->comm(), partition->debug());

      overlapPartition->setChart(partition->getChart());
      ALE::Pullback::SimpleCopy::copy(sendOverlap, recvOverlap, partition, overlapPartition);
      // Create renumbering
      const int         rank           = partition->commRank();
      const point_type *localPoints    = partition->restrictPoint(rank);
      const int         numLocalPoints = partition->getFiberDimension(rank);

      for(point_type p = 0; p < numLocalPoints; ++p) {
        renumbering[localPoints[p]] = p;
      }
      const Obj<typename part_recv_overlap_type::traits::baseSequence> rPoints    = recvOverlap->base();
      point_type                                                       localPoint = numLocalPoints;

      for(typename part_recv_overlap_type::traits::baseSequence::iterator p_iter = rPoints->begin(); p_iter != rPoints->end(); ++p_iter) {
        const Obj<typename part_recv_overlap_type::coneSequence>& ranks           = recvOverlap->cone(*p_iter);
        const rank_type&                                          remotePartPoint = ranks->begin().color();
        const point_type                                         *points          = overlapPartition->restrictPoint(remotePartPoint);
        const int                                                 numPoints       = overlapPartition->getFiberDimension(remotePartPoint);

        for(int i = 0; i < numPoints; ++i) {
          renumbering[points[i]] = localPoint++;
        }
      }
      // Create mesh overlap from partition overlap
      //   TODO: Generalize to redistribution (receive from multiple sources)
      Partitioner::createDistributionMeshOverlap(partition, recvOverlap, renumbering, overlapPartition, sendMeshOverlap, recvMeshOverlap);
      // Send cones
      typedef ALE::Section<point_type, point_type>        cones_type;
      typedef ALE::ConeSection<typename Mesh::sieve_type> cones_wrapper_type;
      Obj<cones_wrapper_type> cones        = new cones_wrapper_type(mesh->getSieve());
      Obj<cones_type>         overlapCones = new cones_type(mesh->comm(), mesh->debug());

      ALE::Pullback::SimpleCopy::copy(sendMeshOverlap, recvMeshOverlap, cones, overlapCones);
      if (mesh->debug()) {overlapCones->view("Overlap Cones");}
      // Inserts cones into parallelMesh (must renumber here)
      ALE::Pullback::InsertionBinaryFusion::fuse(overlapCones, recvMeshOverlap, renumbering, newMesh->getSieve());
    };
    template<typename NewMesh, typename Renumbering, typename SendOverlap, typename RecvOverlap>
    static Obj<partition_type> distributeMesh(const Obj<Mesh>& mesh, const Obj<NewMesh>& newMesh, Renumbering& renumbering, const Obj<SendOverlap>& sendMeshOverlap, const Obj<RecvOverlap>& recvMeshOverlap, const int height = 0) {
      const Obj<partition_type> cellPartition = new partition_type(mesh->comm(), 0, mesh->commSize(), mesh->debug());
      const Obj<partition_type> partition     = new partition_type(mesh->comm(), 0, mesh->commSize(), mesh->debug());

      // Create the cell partition
      Partitioner::createPartition(mesh, cellPartition, height);
      if (mesh->debug()) {
        PetscViewer    viewer;
        PetscErrorCode ierr;

        cellPartition->view("Cell Partition");
        ierr = PetscViewerCreate(mesh->comm(), &viewer);CHKERRXX(ierr);
        ierr = PetscViewerSetType(viewer, PETSC_VIEWER_ASCII);CHKERRXX(ierr);
        ierr = PetscViewerFileSetName(viewer, "mesh.vtk");CHKERRXX(ierr);
        ierr = MeshView_Sieve_Ascii(mesh, cellPartition, viewer);CHKERRXX(ierr);
        ierr = PetscViewerDestroy(viewer);CHKERRXX(ierr);
      }
      // Close the partition over sieve points
      Partitioner::createPartitionClosure(mesh, cellPartition, partition, height);
      if (mesh->debug()) {partition->view("Partition");}
      // Create the remote meshes
      completeMesh(mesh, partition, renumbering, newMesh, sendMeshOverlap, recvMeshOverlap);
      // Create the local mesh
      Partitioner::createLocalMesh(mesh, partition, renumbering, newMesh, height);
      newMesh->stratify();
      return partition;
    };
  };
  template<typename Bundle_>
  class Distribution {
  public:
    typedef Bundle_                                                                     bundle_type;
    typedef typename bundle_type::sieve_type                                            sieve_type;
    typedef typename bundle_type::point_type                                            point_type;
    typedef typename bundle_type::alloc_type                                            alloc_type;
    typedef typename ALE::New::Completion<bundle_type, typename sieve_type::point_type>                            sieveCompletion;
    typedef typename ALE::New::SectionCompletion<bundle_type, typename bundle_type::real_section_type::value_type> sectionCompletion;
    typedef typename sectionCompletion::send_overlap_type                               send_overlap_type;
    typedef typename sectionCompletion::recv_overlap_type                               recv_overlap_type;
  public:
    #undef __FUNCT__
    #define __FUNCT__ "createPartitionOverlap"
    static void createPartitionOverlap(const Obj<bundle_type>& bundle, const Obj<send_overlap_type>& sendOverlap, const Obj<recv_overlap_type>& recvOverlap) {
      const Obj<send_overlap_type>& topSendOverlap = bundle->getSendOverlap();
      const Obj<recv_overlap_type>& topRecvOverlap = bundle->getRecvOverlap();
      const Obj<typename send_overlap_type::traits::baseSequence> base = topSendOverlap->base();
      const Obj<typename recv_overlap_type::traits::capSequence>  cap  = topRecvOverlap->cap();
      const int rank = bundle->commRank();

      if (base->empty()) {
        if (rank == 0) {
          for(int p = 1; p < bundle->commSize(); p++) {
            // The arrow is from local partition point p (source) to remote partition point p (color) on rank p (target)
            sendOverlap->addCone(p, p, p);
          }
        }
      } else {
        for(typename send_overlap_type::traits::baseSequence::iterator b_iter = base->begin(); b_iter != base->end(); ++b_iter) {
          const int& p = *b_iter;
          // The arrow is from local partition point p (source) to remote partition point p (color) on rank p (target)
          sendOverlap->addCone(p, p, p);
        }
      }
      if (cap->empty()) {
        if (rank != 0) {
          // The arrow is from local partition point rank (color) on rank 0 (source) to remote partition point rank (target)
          recvOverlap->addCone(0, rank, rank);
        }
      } else {
        for(typename recv_overlap_type::traits::capSequence::iterator c_iter = cap->begin(); c_iter != cap->end(); ++c_iter) {
          const int& p = *c_iter;
          // The arrow is from local partition point rank (color) on rank p (source) to remote partition point rank (target)
          recvOverlap->addCone(p, rank, rank);
        }
      }
    };
    #undef __FUNCT__
    #define __FUNCT__ "createAssignment"
    template<typename Partitioner>
    static typename Partitioner::part_type *createAssignment(const Obj<bundle_type>& bundle, const int dim, const Obj<send_overlap_type>& sendOverlap, const Obj<recv_overlap_type>& recvOverlap, const int height = 0) {
      // 1) Form partition point overlap a priori
      createPartitionOverlap(bundle, sendOverlap, recvOverlap);
      if (bundle->debug()) {
        sendOverlap->view("Send overlap for partition");
        recvOverlap->view("Receive overlap for partition");
      }
      // 2) Partition the mesh
      if (height == 0) {
        return Partitioner::partitionSieve(bundle, dim);
      } else if (height == 1) {
        return Partitioner::partitionSieveByFace(bundle, dim);
      }
      throw ALE::Exception("Invalid partition height");
    };
    #undef __FUNCT__
    #define __FUNCT__ "scatterBundle"
    // Partition a bundle on process 0 and scatter to all processes
    static void scatterBundle(const Obj<bundle_type>& bundle, const int dim, const Obj<bundle_type>& bundleNew, const Obj<send_overlap_type>& sendOverlap, const Obj<recv_overlap_type>& recvOverlap, const std::string& partitioner, const int height = 0, const Obj<bundle_type>& subBundle = NULL, const Obj<bundle_type>& subBundleNew = NULL) {
      if (height == 0) {
        if (partitioner == "chaco") {
#ifdef PETSC_HAVE_CHACO
          typedef typename ALE::New::Chaco::Partitioner<bundle_type> Partitioner;
          typedef typename ALE::New::Partitioner<bundle_type>        GenPartitioner;
          typedef typename Partitioner::part_type                    part_type;

          part_type *assignment = scatterBundle<Partitioner>(bundle, dim, bundleNew, sendOverlap, recvOverlap, height);
          if (!subBundle.isNull() && !subBundleNew.isNull()) {
            part_type *subAssignment = GenPartitioner::subordinatePartition(bundle, 1, subBundle, assignment);
            const Obj<sieve_type>& sieve      = subBundle->getSieve();
            const Obj<sieve_type>& sieveNew   = new Mesh::sieve_type(subBundle->comm(), subBundle->debug());
            const int              numCells   = subBundle->heightStratum(height)->size();

            subBundleNew->setSieve(sieveNew);
            sieveCompletion::scatterSieve(subBundle, sieve, dim, sieveNew, sendOverlap, recvOverlap, height, numCells, subAssignment);
            subBundleNew->stratify();
            if (subAssignment != NULL) delete [] subAssignment;
          }
          if (assignment != NULL) delete [] assignment;
#else
          throw ALE::Exception("Chaco is not installed. Reconfigure with the flag --download-chaco");
#endif
        } else if (partitioner == "parmetis") {
#ifdef PETSC_HAVE_PARMETIS
          typedef typename ALE::New::ParMetis::Partitioner<bundle_type> Partitioner;
          typedef typename ALE::New::Partitioner<bundle_type>           GenPartitioner;
          typedef typename Partitioner::part_type                       part_type;

          part_type *assignment = scatterBundle<Partitioner>(bundle, dim, bundleNew, sendOverlap, recvOverlap, height);
          if (!subBundle.isNull() && !subBundleNew.isNull()) {
            part_type *subAssignment = GenPartitioner::subordinatePartition(bundle, 1, subBundle, assignment);
            const Obj<sieve_type>& sieve      = subBundle->getSieve();
            const Obj<sieve_type>& sieveNew   = new Mesh::sieve_type(subBundle->comm(), subBundle->debug());
            const int              numCells   = subBundle->heightStratum(height)->size();

            subBundleNew->setSieve(sieveNew);
            sieveCompletion::scatterSieve(subBundle, sieve, dim, sieveNew, sendOverlap, recvOverlap, height, numCells, subAssignment);
            subBundleNew->stratify();
            if (subAssignment != NULL) delete [] subAssignment;
          }
          if (assignment != NULL) delete [] assignment;
#else
          throw ALE::Exception("ParMetis is not installed. Reconfigure with the flag --download-parmetis");
#endif
        } else {
          throw ALE::Exception("Unknown partitioner");
        }
      } else if (height == 1) {
        if (partitioner == "zoltan") {
#ifdef PETSC_HAVE_ZOLTAN
          typedef typename ALE::New::Zoltan::Partitioner<bundle_type> Partitioner;
          typedef typename Partitioner::part_type                     part_type;

          part_type *assignment = scatterBundle<Partitioner>(bundle, dim, bundleNew, sendOverlap, recvOverlap, height);
          if (assignment != NULL) delete [] assignment;
#else
          throw ALE::Exception("Zoltan is not installed. Reconfigure with the flag --download-zoltan");
#endif
        } else if (partitioner == "parmetis") {
#ifdef PETSC_HAVE_PARMETIS
          typedef typename ALE::New::ParMetis::Partitioner<bundle_type> Partitioner;
          typedef typename Partitioner::part_type                       part_type;

          part_type *assignment = scatterBundle<Partitioner>(bundle, dim, bundleNew, sendOverlap, recvOverlap, height);
          if (assignment != NULL) delete [] assignment;
#else
          throw ALE::Exception("ParMetis is not installed. Reconfigure with the flag --download-parmetis");
#endif
        } else {
          throw ALE::Exception("Unknown partitioner");
        }
      } else {
        throw ALE::Exception("Invalid partition height");
      }
    };
    template<typename Partitioner>
    static typename Partitioner::part_type *scatterBundle(const Obj<bundle_type>& bundle, const int dim, const Obj<bundle_type>& bundleNew, const Obj<send_overlap_type>& sendOverlap, const Obj<recv_overlap_type>& recvOverlap, const int height = 0) {
      typename Partitioner::part_type *assignment = createAssignment<Partitioner>(bundle, dim, sendOverlap, recvOverlap, height);
      const Obj<sieve_type>&           sieve      = bundle->getSieve();
      const Obj<sieve_type>&           sieveNew   = bundleNew->getSieve();
      const int                        numPoints  = bundle->heightStratum(height)->size();

      sieveCompletion::scatterSieve(bundle, sieve, dim, sieveNew, sendOverlap, recvOverlap, height, numPoints, assignment);
      bundleNew->stratify();
      return assignment;
    };
    #undef __FUNCT__
    #define __FUNCT__ "distributeMesh"
    static Obj<ALE::Mesh> distributeMesh(const Obj<ALE::Mesh>& serialMesh, const int height = 0, const std::string& partitioner = "chaco") {
      MPI_Comm                          comm          = serialMesh->comm();
      const int                         dim           = serialMesh->getDimension();
      Obj<ALE::Mesh>                    parallelMesh  = new ALE::Mesh(comm, dim, serialMesh->debug());
      const Obj<ALE::Mesh::sieve_type>& parallelSieve = new ALE::Mesh::sieve_type(comm, serialMesh->debug());

      ALE_LOG_EVENT_BEGIN;
      parallelMesh->setSieve(parallelSieve);
      if (serialMesh->debug()) {serialMesh->view("Serial mesh");}

      // Distribute cones
      Obj<send_overlap_type> sendOverlap = new send_overlap_type(comm, serialMesh->debug());
      Obj<recv_overlap_type> recvOverlap = new recv_overlap_type(comm, serialMesh->debug());
      scatterBundle(serialMesh, dim, parallelMesh, sendOverlap, recvOverlap, partitioner, height);
      parallelMesh->setDistSendOverlap(sendOverlap);
      parallelMesh->setDistRecvOverlap(recvOverlap);

      // Distribute labels
      const typename bundle_type::labels_type& labels = serialMesh->getLabels();

      for(typename bundle_type::labels_type::const_iterator l_iter = labels.begin(); l_iter != labels.end(); ++l_iter) {
        if (parallelMesh->hasLabel(l_iter->first)) continue;
        const Obj<typename bundle_type::label_type>& serialLabel   = l_iter->second;
        const Obj<typename bundle_type::label_type>& parallelLabel = parallelMesh->createLabel(l_iter->first);
        // Create local label
#define NEW_LABEL
#ifdef NEW_LABEL
        parallelLabel->add(serialLabel, parallelSieve);
#else
        const Obj<typename bundle_type::label_type::traits::baseSequence>& base = serialLabel->base();

        for(typename bundle_type::label_type::traits::baseSequence::iterator b_iter = base->begin(); b_iter != base->end(); ++b_iter) {
          if (parallelSieve->capContains(*b_iter) || parallelSieve->baseContains(*b_iter)) {
            parallelLabel->addArrow(*serialLabel->cone(*b_iter)->begin(), *b_iter);
          }
        }
#endif
        // Get remote labels
        sieveCompletion::scatterCones(serialLabel, parallelLabel, sendOverlap, recvOverlap);
      }

      // Distribute sections
      Obj<std::set<std::string> > sections = serialMesh->getRealSections();

      for(std::set<std::string>::iterator name = sections->begin(); name != sections->end(); ++name) {
        parallelMesh->setRealSection(*name, distributeSection(serialMesh->getRealSection(*name), parallelMesh, sendOverlap, recvOverlap));
      }
      sections = serialMesh->getIntSections();
      for(std::set<std::string>::iterator name = sections->begin(); name != sections->end(); ++name) {
        parallelMesh->setIntSection(*name, distributeSection(serialMesh->getIntSection(*name), parallelMesh, sendOverlap, recvOverlap));
      }
      sections = serialMesh->getArrowSections();

      for(std::set<std::string>::iterator name = sections->begin(); name != sections->end(); ++name) {
        parallelMesh->setArrowSection(*name, distributeArrowSection(serialMesh->getArrowSection(*name), serialMesh, parallelMesh, sendOverlap, recvOverlap));
      }
      if (parallelMesh->debug()) {parallelMesh->view("Parallel Mesh");}
      ALE_LOG_EVENT_END;
      return parallelMesh;
    };
    #undef __FUNCT__
    #define __FUNCT__ "updateSectionLocal"
    template<typename Section>
    static void updateSectionLocal(const Obj<Section>& oldSection, const Obj<bundle_type>& newBundle, const Obj<Section>& newSection) {
      const Obj<typename bundle_type::sieve_type>&    newSieve = newBundle->getSieve();
      const typename Section::atlas_type::chart_type& oldChart = oldSection->getChart();

      for(typename Section::atlas_type::chart_type::const_iterator c_iter = oldChart.begin(); c_iter != oldChart.end(); ++c_iter) {
        if (newSieve->capContains(*c_iter) || newSieve->baseContains(*c_iter)) {
          newSection->setFiberDimension(*c_iter, oldSection->getFiberDimension(*c_iter));
        }
      }
      newBundle->allocate(newSection);
      const typename Section::atlas_type::chart_type& newChart = newSection->getChart();

      for(typename Section::atlas_type::chart_type::const_iterator c_iter = newChart.begin(); c_iter != newChart.end(); ++c_iter) {
        newSection->updatePointAll(*c_iter, oldSection->restrictPoint(*c_iter));
      }
    };
    #undef __FUNCT__
    #define __FUNCT__ "updateSectionRemote"
    template<typename RecvSection, typename Section>
    static void updateSectionRemote(const Obj<recv_overlap_type>& recvOverlap, const Obj<RecvSection>& recvSection, const Obj<bundle_type>& newBundle, const Obj<Section>& newSection) {
      Obj<typename recv_overlap_type::traits::baseSequence> recvPoints = recvOverlap->base();

      for(typename recv_overlap_type::traits::baseSequence::iterator r_iter = recvPoints->begin(); r_iter != recvPoints->end(); ++r_iter) {
        const Obj<typename recv_overlap_type::traits::coneSequence>&     recvPatches = recvOverlap->cone(*r_iter);
        const typename recv_overlap_type::traits::coneSequence::iterator end         = recvPatches->end();

        for(typename recv_overlap_type::traits::coneSequence::iterator p_iter = recvPatches->begin(); p_iter != end; ++p_iter) {
          newSection->addPoint(*r_iter, recvSection->getSection(*p_iter)->getFiberDimension(*r_iter));
        }
      }
      newBundle->reallocate(newSection);
      for(typename recv_overlap_type::traits::baseSequence::iterator r_iter = recvPoints->begin(); r_iter != recvPoints->end(); ++r_iter) {
        const Obj<typename recv_overlap_type::traits::coneSequence>&     recvPatches = recvOverlap->cone(*r_iter);
        const typename recv_overlap_type::traits::coneSequence::iterator end         = recvPatches->end();

        for(typename recv_overlap_type::traits::coneSequence::iterator p_iter = recvPatches->begin(); p_iter != end; ++p_iter) {
          if (recvSection->getSection(*p_iter)->getFiberDimension(*r_iter)) {
            newSection->updatePointAll(*r_iter, recvSection->getSection(*p_iter)->restrictPoint(*r_iter));
          }
        }
      }
    };
    #undef __FUNCT__
    #define __FUNCT__ "distributeSection"
    template<typename Section>
    static Obj<Section> distributeSection(const Obj<Section>& serialSection, const Obj<bundle_type>& parallelBundle, const Obj<send_overlap_type>& sendOverlap, const Obj<recv_overlap_type>& recvOverlap) {
      if (serialSection->debug()) {
        serialSection->view("Serial Section");
      }
      typedef typename alloc_type::template rebind<typename Section::value_type>::other value_alloc_type;
      typedef ALE::Field<send_overlap_type, int, ALE::Section<point_type, typename Section::value_type, value_alloc_type> > send_section_type;
      typedef ALE::Field<recv_overlap_type, int, ALE::Section<point_type, typename Section::value_type, value_alloc_type> > recv_section_type;
      typedef ALE::New::SizeSection<Section> SectionSizer;
      Obj<Section>                 parallelSection = new Section(serialSection->comm(), serialSection->debug());
      const Obj<send_section_type> sendSection     = new send_section_type(serialSection->comm(), serialSection->debug());
      const Obj<recv_section_type> recvSection     = new recv_section_type(serialSection->comm(), sendSection->getTag(), serialSection->debug());
      const Obj<SectionSizer>      sizer           = new SectionSizer(serialSection);

      updateSectionLocal(serialSection, parallelBundle, parallelSection);
      sectionCompletion::completeSection(sendOverlap, recvOverlap, sizer, serialSection, sendSection, recvSection);
      updateSectionRemote(recvOverlap, recvSection, parallelBundle, parallelSection);
      if (parallelSection->debug()) {
        parallelSection->view("Parallel Section");
      }
      return parallelSection;
    };
    #undef __FUNCT__
    #define __FUNCT__ "updateArrowSectionLocal"
    template<typename Section>
    static void updateArrowSectionLocal(const Obj<Section>& oldSection, const Obj<bundle_type>& newBundle, const Obj<Section>& newSection) {
      const Obj<typename bundle_type::sieve_type>&    newSieve = newBundle->getSieve();
      const typename Section::atlas_type::chart_type& oldChart = oldSection->getChart();

      for(typename Section::atlas_type::chart_type::const_iterator c_iter = oldChart.begin(); c_iter != oldChart.end(); ++c_iter) {
        // Dmitry should provide a Sieve::contains(MinimalArrow) method
        if (newSieve->capContains(c_iter->source) && newSieve->baseContains(c_iter->target)) {
          newSection->setFiberDimension(*c_iter, oldSection->getFiberDimension(*c_iter));
        }
      }
      //newBundle->allocate(newSection);
      const typename Section::atlas_type::chart_type& newChart = newSection->getChart();

      for(typename Section::atlas_type::chart_type::const_iterator c_iter = newChart.begin(); c_iter != newChart.end(); ++c_iter) {
        newSection->updatePointAll(*c_iter, oldSection->restrictPoint(*c_iter));
      }
    };
    #undef __FUNCT__
    #define __FUNCT__ "updateArrowSectionRemote"
    template<typename RecvSection, typename Section>
    static void updateArrowSectionRemote(const Obj<recv_overlap_type>& recvOverlap, const Obj<RecvSection>& recvSection, const Obj<bundle_type>& newBundle, const Obj<Section>& newSection) {
      Obj<typename recv_overlap_type::traits::baseSequence> recvPoints = recvOverlap->base();

      for(typename recv_overlap_type::traits::baseSequence::iterator r_iter = recvPoints->begin(); r_iter != recvPoints->end(); ++r_iter) {
        const Obj<typename bundle_type::sieve_type::traits::coneSequence>&     cone = newBundle->getSieve()->cone(*r_iter);
        const typename bundle_type::sieve_type::traits::coneSequence::iterator end  = cone->end();

        for(typename bundle_type::sieve_type::traits::coneSequence::iterator c_iter = cone->begin(); c_iter != end; ++c_iter) {
          newSection->setFiberDimension(typename Section::point_type(*c_iter, *r_iter), 1);
        }
      }
      //newBundle->reallocate(newSection);
      for(typename recv_overlap_type::traits::baseSequence::iterator r_iter = recvPoints->begin(); r_iter != recvPoints->end(); ++r_iter) {
        const Obj<typename recv_overlap_type::traits::coneSequence>&     recvPatches = recvOverlap->cone(*r_iter);
        const typename recv_overlap_type::traits::coneSequence::iterator recvEnd     = recvPatches->end();

        for(typename recv_overlap_type::traits::coneSequence::iterator p_iter = recvPatches->begin(); p_iter != recvEnd; ++p_iter) {
          const Obj<typename RecvSection::section_type>& section = recvSection->getSection(*p_iter);

          if (section->getFiberDimension(*r_iter)) {
            const Obj<typename bundle_type::sieve_type::traits::coneSequence>&     cone    = newBundle->getSieve()->cone(*r_iter);
            const typename bundle_type::sieve_type::traits::coneSequence::iterator end     = cone->end();
            const typename RecvSection::value_type                                *values  = section->restrictPoint(*r_iter);
            int                                                                    c       = -1;

            for(typename bundle_type::sieve_type::traits::coneSequence::iterator c_iter = cone->begin(); c_iter != end; ++c_iter) {
              newSection->updatePoint(typename Section::point_type(*c_iter, *r_iter), &values[++c]);
            }
          }
        }
      }
    };
    #undef __FUNCT__
    #define __FUNCT__ "distributeArrowSection"
    template<typename Section>
    static Obj<Section> distributeArrowSection(const Obj<Section>& serialSection, const Obj<bundle_type>& serialBundle, const Obj<bundle_type>& parallelBundle, const Obj<send_overlap_type>& sendOverlap, const Obj<recv_overlap_type>& recvOverlap) {
      if (serialSection->debug()) {
        serialSection->view("Serial ArrowSection");
      }
      typedef typename alloc_type::template rebind<typename Section::value_type>::other value_alloc_type;
      typedef ALE::Field<send_overlap_type, int, ALE::Section<point_type, typename Section::value_type, value_alloc_type> > send_section_type;
      typedef ALE::Field<recv_overlap_type, int, ALE::Section<point_type, typename Section::value_type, value_alloc_type> > recv_section_type;
      typedef ALE::New::ConeSizeSection<bundle_type, sieve_type> SectionSizer;
      typedef ALE::New::ArrowSection<sieve_type, Section>        ArrowFiller;
      Obj<Section>                 parallelSection = new Section(serialSection->comm(), serialSection->debug());
      const Obj<send_section_type> sendSection     = new send_section_type(serialSection->comm(), serialSection->debug());
      const Obj<recv_section_type> recvSection     = new recv_section_type(serialSection->comm(), sendSection->getTag(), serialSection->debug());
      const Obj<SectionSizer>      sizer           = new SectionSizer(serialBundle, serialBundle->getSieve());
      const Obj<ArrowFiller>       filler          = new ArrowFiller(serialBundle->getSieve(), serialSection);

      updateArrowSectionLocal(serialSection, parallelBundle, parallelSection);
      sectionCompletion::completeSection(sendOverlap, recvOverlap, sizer, filler, sendSection, recvSection);
      updateArrowSectionRemote(recvOverlap, recvSection, parallelBundle, parallelSection);
      if (parallelSection->debug()) {
        parallelSection->view("Parallel ArrowSection");
      }
      return parallelSection;
    };
    static void unifyBundle(const Obj<bundle_type>& bundle, const int dim, const Obj<bundle_type>& bundleNew, const Obj<send_overlap_type>& sendOverlap, const Obj<recv_overlap_type>& recvOverlap) {
      typedef int part_type;
      const Obj<sieve_type>& sieve    = bundle->getSieve();
      const Obj<sieve_type>& sieveNew = bundleNew->getSieve();
      const int              rank     = bundle->commRank();
      const int              debug    = bundle->debug();

      // 1) Form partition point overlap a priori
      if (rank == 0) {
        for(int p = 1; p < sieve->commSize(); p++) {
          // The arrow is from remote partition point 0 on rank p to local partition point 0
          recvOverlap->addCone(p, 0, 0);
        }
      } else {
        // The arrow is from local partition point 0 to remote partition point 0 on rank 0
        sendOverlap->addCone(0, 0, 0);
      }
      if (debug) {
        sendOverlap->view("Send overlap for partition");
        recvOverlap->view("Receive overlap for partition");
      }
      // 2) Partition the mesh
      int        numCells   = bundle->heightStratum(0)->size();
      part_type *assignment = new part_type[numCells];

      for(int c = 0; c < numCells; ++c) {
        assignment[c] = 0;
      }
      // 3) Scatter the sieve
      sieveCompletion::scatterSieve(bundle, sieve, dim, sieveNew, sendOverlap, recvOverlap, 0, numCells, assignment);
      bundleNew->stratify();
      // 4) Cleanup
      if (assignment != NULL) delete [] assignment;
    };
    #undef __FUNCT__
    #define __FUNCT__ "unifyMesh"
    static Obj<ALE::Mesh> unifyMesh(const Obj<ALE::Mesh>& parallelMesh) {
      const int                         dim         = parallelMesh->getDimension();
      Obj<ALE::Mesh>                    serialMesh  = new ALE::Mesh(parallelMesh->comm(), dim, parallelMesh->debug());
      const Obj<ALE::Mesh::sieve_type>& serialSieve = new ALE::Mesh::sieve_type(parallelMesh->comm(), parallelMesh->debug());

      ALE_LOG_EVENT_BEGIN;
      serialMesh->setSieve(serialSieve);
      if (parallelMesh->debug()) {
        parallelMesh->view("Parallel topology");
      }

      // Unify cones
      Obj<send_overlap_type> sendOverlap = new send_overlap_type(serialMesh->comm(), serialMesh->debug());
      Obj<recv_overlap_type> recvOverlap = new recv_overlap_type(serialMesh->comm(), serialMesh->debug());
      unifyBundle(parallelMesh, dim, serialMesh, sendOverlap, recvOverlap);
      serialMesh->setDistSendOverlap(sendOverlap);
      serialMesh->setDistRecvOverlap(recvOverlap);

      // Unify labels
      const typename bundle_type::labels_type& labels = parallelMesh->getLabels();

      for(typename bundle_type::labels_type::const_iterator l_iter = labels.begin(); l_iter != labels.end(); ++l_iter) {
        if (serialMesh->hasLabel(l_iter->first)) continue;
        const Obj<typename bundle_type::label_type>& parallelLabel = l_iter->second;
        const Obj<typename bundle_type::label_type>& serialLabel   = serialMesh->createLabel(l_iter->first);

        sieveCompletion::scatterCones(parallelLabel, serialLabel, sendOverlap, recvOverlap);
      }

      // Unify coordinates
      Obj<std::set<std::string> > sections = parallelMesh->getRealSections();

      for(std::set<std::string>::iterator name = sections->begin(); name != sections->end(); ++name) {
        serialMesh->setRealSection(*name, distributeSection(parallelMesh->getRealSection(*name), serialMesh, sendOverlap, recvOverlap));
      }
      sections = parallelMesh->getIntSections();
      for(std::set<std::string>::iterator name = sections->begin(); name != sections->end(); ++name) {
        serialMesh->setIntSection(*name, distributeSection(parallelMesh->getIntSection(*name), serialMesh, sendOverlap, recvOverlap));
      }
      sections = parallelMesh->getArrowSections();
      for(std::set<std::string>::iterator name = sections->begin(); name != sections->end(); ++name) {
        serialMesh->setArrowSection(*name, distributeArrowSection(parallelMesh->getArrowSection(*name), parallelMesh, serialMesh, sendOverlap, recvOverlap));
      }
      if (serialMesh->debug()) {serialMesh->view("Serial Mesh");}
      ALE_LOG_EVENT_END;
      return serialMesh;
    };
  public: // Do not like these
    #undef __FUNCT__
    #define __FUNCT__ "updateOverlap"
    // This is just crappy. We could introduce another phase to find out exactly what
    //   indices people do not have in the global order after communication
    template<typename SendSection, typename RecvSection>
    static void updateOverlap(const Obj<SendSection>& sendSection, const Obj<RecvSection>& recvSection, const Obj<send_overlap_type>& sendOverlap, const Obj<recv_overlap_type>& recvOverlap) {
      const typename SendSection::sheaf_type& sendRanks = sendSection->getPatches();
      const typename RecvSection::sheaf_type& recvRanks = recvSection->getPatches();

      for(typename SendSection::sheaf_type::const_iterator p_iter = sendRanks.begin(); p_iter != sendRanks.end(); ++p_iter) {
        const typename SendSection::patch_type&               rank    = p_iter->first;
        const Obj<typename SendSection::section_type>&        section = p_iter->second;
        const typename SendSection::section_type::chart_type& chart   = section->getChart();

        for(typename SendSection::section_type::chart_type::const_iterator b_iter = chart.begin(); b_iter != chart.end(); ++b_iter) {
          const typename SendSection::value_type *points = section->restrictPoint(*b_iter);
          const int                               size   = section->getFiberDimension(*b_iter);

          for(int p = 0; p < size; p++) {
            sendOverlap->addArrow(points[p], rank, points[p]);
          }
        }
      }
      for(typename RecvSection::sheaf_type::const_iterator p_iter = recvRanks.begin(); p_iter != recvRanks.end(); ++p_iter) {
        const typename RecvSection::patch_type&               rank    = p_iter->first;
        const Obj<typename RecvSection::section_type>&        section = p_iter->second;
        const typename RecvSection::section_type::chart_type& chart   = section->getChart();

        for(typename RecvSection::section_type::chart_type::const_iterator b_iter = chart.begin(); b_iter != chart.end(); ++b_iter) {
          const typename RecvSection::value_type *points = section->restrictPoint(*b_iter);
          const int                               size   = section->getFiberDimension(*b_iter);

          for(int p = 0; p < size; p++) {
            recvOverlap->addArrow(rank, points[p], points[p]);
          }
        }
      }
    };
    #undef __FUNCT__
    #define __FUNCT__ "updateSieve"
    template<typename RecvSection>
    static void updateSieve(const Obj<RecvSection>& recvSection, const Obj<sieve_type>& sieve) {
      const typename RecvSection::sheaf_type& ranks = recvSection->getPatches();

      for(typename RecvSection::sheaf_type::const_iterator p_iter = ranks.begin(); p_iter != ranks.end(); ++p_iter) {
        const Obj<typename RecvSection::section_type>&        section = p_iter->second;
        const typename RecvSection::section_type::chart_type& chart   = section->getChart();

        for(typename RecvSection::section_type::chart_type::const_iterator b_iter = chart.begin(); b_iter != chart.end(); ++b_iter) {
          const typename RecvSection::value_type *points = section->restrictPoint(*b_iter);
          int                                     size   = section->getFiberDimension(*b_iter);
          int                                     c      = 0;

          for(int p = 0; p < size; p++) {
            //sieve->addArrow(points[p], *b_iter, c++);
            sieve->addArrow(points[p], *b_iter, c);
          }
        }
      }
    };
    #undef __FUNCT__
    #define __FUNCT__ "coneCompletion"
    template<typename SendSection, typename RecvSection>
    static void coneCompletion(const Obj<send_overlap_type>& sendOverlap, const Obj<recv_overlap_type>& recvOverlap, const Obj<bundle_type>& bundle, const Obj<SendSection>& sendSection, const Obj<RecvSection>& recvSection) {
      if (sendOverlap->commSize() == 1) return;
      // Distribute cones
      const Obj<sieve_type>&                                 sieve           = bundle->getSieve();
      const Obj<typename sieveCompletion::topology_type>     secTopology     = sieveCompletion::completion::createSendTopology(sendOverlap);
      const Obj<typename sieveCompletion::cone_size_section> coneSizeSection = new typename sieveCompletion::cone_size_section(bundle, sieve);
      const Obj<typename sieveCompletion::cone_section>      coneSection     = new typename sieveCompletion::cone_section(sieve);
      sieveCompletion::completion::completeSection(sendOverlap, recvOverlap, coneSizeSection, coneSection, sendSection, recvSection);
      // Update cones
      updateSieve(recvSection, sieve);
    };
    #undef __FUNCT__
    #define __FUNCT__ "completeSection"
    template<typename Section>
    static void completeSection(const Obj<bundle_type>& bundle, const Obj<Section>& section) {
      typedef typename Distribution<bundle_type>::sieveCompletion sieveCompletion;
      typedef typename bundle_type::send_overlap_type             send_overlap_type;
      typedef typename bundle_type::recv_overlap_type             recv_overlap_type;
      typedef typename Section::value_type                        value_type;
      typedef typename alloc_type::template rebind<typename Section::value_type>::other value_alloc_type;
      typedef typename ALE::Field<send_overlap_type, int, ALE::Section<point_type, value_type, value_alloc_type> > send_section_type;
      typedef typename ALE::Field<recv_overlap_type, int, ALE::Section<point_type, value_type, value_alloc_type> > recv_section_type;
      typedef ALE::New::SizeSection<Section>                                SectionSizer;
      const int debug = section->debug();

      bundle->constructOverlap();
      const Obj<send_overlap_type> sendOverlap = bundle->getSendOverlap();
      const Obj<recv_overlap_type> recvOverlap = bundle->getRecvOverlap();
      const Obj<send_section_type> sendSection = new send_section_type(section->comm(), section->debug());
      const Obj<recv_section_type> recvSection = new recv_section_type(section->comm(), sendSection->getTag(), section->debug());
      const Obj<SectionSizer>      sizer       = new SectionSizer(section);

      sectionCompletion::completeSection(sendOverlap, recvOverlap, sizer, section, sendSection, recvSection);
      // Update section with remote data
      const Obj<typename recv_overlap_type::traits::baseSequence> recvPoints = bundle->getRecvOverlap()->base();

      for(typename recv_overlap_type::traits::baseSequence::iterator r_iter = recvPoints->begin(); r_iter != recvPoints->end(); ++r_iter) {
        const Obj<typename recv_overlap_type::traits::coneSequence>&     recvPatches = recvOverlap->cone(*r_iter);
        const typename recv_overlap_type::traits::coneSequence::iterator end         = recvPatches->end();

        for(typename recv_overlap_type::traits::coneSequence::iterator p_iter = recvPatches->begin(); p_iter != end; ++p_iter) {
          if (recvSection->getSection(*p_iter)->getFiberDimension(p_iter.color())) {
            if (debug) {std::cout << "["<<section->commRank()<<"]Completed point " << *r_iter << std::endl;}
            section->updateAddPoint(*r_iter, recvSection->getSection(*p_iter)->restrictPoint(p_iter.color()));
          }
        }
      }
    };
  };
}

namespace ALECompat {
  namespace New {
    template<typename Topology_>
    class Distribution {
    public:
      typedef Topology_                                                                   topology_type;
      typedef typename topology_type::sieve_type                                          sieve_type;
      typedef ALECompat::New::Completion<Topology_, Mesh::sieve_type::point_type>               sieveCompletion;
      typedef ALECompat::New::SectionCompletion<Topology_, Mesh::real_section_type::value_type> sectionCompletion;
      typedef typename sectionCompletion::send_overlap_type                               send_overlap_type;
      typedef typename sectionCompletion::recv_overlap_type                               recv_overlap_type;
    public:
      #undef __FUNCT__
      #define __FUNCT__ "updateOverlap"
      // This is just crappy. WE could introduce another phase to find out exactly what
      //   indices people do not have in the global order after communication
      template<typename SendSection, typename RecvSection>
      static void updateOverlap(const Obj<SendSection>& sendSection, const Obj<RecvSection>& recvSection, const Obj<send_overlap_type>& sendOverlap, const Obj<recv_overlap_type>& recvOverlap) {
        const typename SendSection::topology_type::sheaf_type& sendRanks = sendSection->getTopology()->getPatches();
        const typename RecvSection::topology_type::sheaf_type& recvRanks = recvSection->getTopology()->getPatches();

        for(typename SendSection::topology_type::sheaf_type::const_iterator p_iter = sendRanks.begin(); p_iter != sendRanks.end(); ++p_iter) {
          int                                                                       rank = p_iter->first;
          const Obj<typename SendSection::topology_type::sieve_type::baseSequence>& base = p_iter->second->base();

          for(typename SendSection::topology_type::sieve_type::baseSequence::iterator b_iter = base->begin(); b_iter != base->end(); ++b_iter) {
            const typename SendSection::value_type *points = sendSection->restrict(rank, *b_iter);
            int size = sendSection->getFiberDimension(rank, *b_iter);

            for(int p = 0; p < size; p++) {
              sendOverlap->addArrow(points[p], rank, points[p]);
            }
          }
        }
        for(typename RecvSection::topology_type::sheaf_type::const_iterator p_iter = recvRanks.begin(); p_iter != recvRanks.end(); ++p_iter) {
          int                                                                       rank = p_iter->first;
          const Obj<typename RecvSection::topology_type::sieve_type::baseSequence>& base = p_iter->second->base();

          for(typename RecvSection::topology_type::sieve_type::baseSequence::iterator b_iter = base->begin(); b_iter != base->end(); ++b_iter) {
            const typename RecvSection::value_type *points = recvSection->restrict(rank, *b_iter);
            int size = recvSection->getFiberDimension(rank, *b_iter);

            for(int p = 0; p < size; p++) {
              recvOverlap->addArrow(rank, points[p], points[p]);
            }
          }
        }
      };
      static void createLabelOverlap(const Obj<send_overlap_type>& sendOverlap, const Obj<recv_overlap_type>& recvOverlap) {
      };
      #undef __FUNCT__
      #define __FUNCT__ "updateSectionLocal"
      template<typename Section>
      static void updateSectionLocal(const Obj<Section>& oldSection, const Obj<Section>& newSection)
      {
        const typename Section::topology_type::sheaf_type& patches = newSection->getTopology()->getPatches();

        for(typename Section::topology_type::sheaf_type::const_iterator p_iter = patches.begin(); p_iter != patches.end(); ++p_iter) {
          const typename Section::patch_type&                     patch    = p_iter->first;
          const Obj<typename Section::topology_type::sieve_type>& newSieve = p_iter->second;
          if (!oldSection->hasPatch(patch)) continue;
          const typename Section::atlas_type::chart_type&         oldChart = oldSection->getPatch(patch);

          for(typename Section::atlas_type::chart_type::const_iterator c_iter = oldChart.begin(); c_iter != oldChart.end(); ++c_iter) {
            if (newSieve->hasPoint(*c_iter)) {
              newSection->setFiberDimension(patch, *c_iter, oldSection->getFiberDimension(patch, *c_iter));
            }
          }
        }
        newSection->allocate();
        for(typename Section::topology_type::sheaf_type::const_iterator p_iter = patches.begin(); p_iter != patches.end(); ++p_iter) {
          const typename Section::patch_type&             patch    = p_iter->first;
          if (!oldSection->hasPatch(patch)) continue;
          const typename Section::atlas_type::chart_type& newChart = newSection->getPatch(patch);

          for(typename Section::atlas_type::chart_type::const_iterator c_iter = newChart.begin(); c_iter != newChart.end(); ++c_iter) {
            newSection->updatePoint(patch, *c_iter, oldSection->restrictPoint(patch, *c_iter));
          }
        }
      };
      #undef __FUNCT__
      #define __FUNCT__ "updateSectionRemote"
      template<typename RecvSection, typename Section>
      static void updateSectionRemote(const Obj<recv_overlap_type>& recvOverlap, const Obj<RecvSection>& recvSection, const Obj<Section>& newSection) {
        const Mesh::real_section_type::patch_type                  patch      = 0;
        Obj<typename recv_overlap_type::traits::baseSequence> recvPoints = recvOverlap->base();

        for(typename recv_overlap_type::traits::baseSequence::iterator r_iter = recvPoints->begin(); r_iter != recvPoints->end(); ++r_iter) {
          const Obj<typename recv_overlap_type::traits::coneSequence>&     recvPatches = recvOverlap->cone(*r_iter);
          const typename recv_overlap_type::traits::coneSequence::iterator end         = recvPatches->end();

          for(typename recv_overlap_type::traits::coneSequence::iterator p_iter = recvPatches->begin(); p_iter != end; ++p_iter) {
            newSection->addPoint(patch, *r_iter, recvSection->getFiberDimension(*p_iter, *r_iter));
          }
        }
        newSection->reallocate();
        for(typename recv_overlap_type::traits::baseSequence::iterator r_iter = recvPoints->begin(); r_iter != recvPoints->end(); ++r_iter) {
          const Obj<typename recv_overlap_type::traits::coneSequence>&     recvPatches = recvOverlap->cone(*r_iter);
          const typename recv_overlap_type::traits::coneSequence::iterator end         = recvPatches->end();

          for(typename recv_overlap_type::traits::coneSequence::iterator p_iter = recvPatches->begin(); p_iter != end; ++p_iter) {
            if (recvSection->getFiberDimension(*p_iter, *r_iter)) {
              newSection->updatePoint(patch, *r_iter, recvSection->restrictPoint(*p_iter, *r_iter));
            }
          }
        }
      };
      #undef __FUNCT__
      #define __FUNCT__ "updateSieve"
      template<typename RecvSection>
      static void updateSieve(const Obj<RecvSection>& recvSection, const Obj<topology_type>& topology) {
        const typename RecvSection::patch_type                 patch = 0;
        const typename RecvSection::topology_type::sheaf_type& ranks = recvSection->getTopology()->getPatches();
        const Obj<typename topology_type::sieve_type>&         sieve = topology->getPatch(patch);

        for(typename RecvSection::topology_type::sheaf_type::const_iterator p_iter = ranks.begin(); p_iter != ranks.end(); ++p_iter) {
          int                                                              rank = p_iter->first;
          const Obj<typename RecvSection::topology_type::sieve_type::baseSequence>& base = p_iter->second->base();

          for(typename RecvSection::topology_type::sieve_type::baseSequence::iterator b_iter = base->begin(); b_iter != base->end(); ++b_iter) {
            const typename RecvSection::value_type *points = recvSection->restrict(rank, *b_iter);
            int size = recvSection->getFiberDimension(rank, *b_iter);
            int c = 0;

            for(int p = 0; p < size; p++) {
              //sieve->addArrow(points[p], *b_iter, c++);
              sieve->addArrow(points[p], *b_iter, c);
            }
          }
        }
      };
       #undef __FUNCT__
      #define __FUNCT__ "coneCompletion"
      template<typename SendSection, typename RecvSection>
      static void coneCompletion(const Obj<send_overlap_type>& sendOverlap, const Obj<recv_overlap_type>& recvOverlap, const Obj<topology_type>& topology, const Obj<SendSection>& sendSection, const Obj<RecvSection>& recvSection) {
        if (sendOverlap->commSize() == 1) return;
        typedef typename ALECompat::New::SectionCompletion<topology_type, typename sieve_type::point_type> completion;

        // Distribute cones
        const typename topology_type::patch_type               patch           = 0;
        const Obj<typename sieveCompletion::topology_type>     secTopology     = completion::createSendTopology(sendOverlap);
        const Obj<typename sieveCompletion::cone_size_section> coneSizeSection = new typename sieveCompletion::cone_size_section(secTopology, topology, topology->getPatch(patch));
        const Obj<typename sieveCompletion::cone_section>      coneSection     = new typename sieveCompletion::cone_section(secTopology, topology->getPatch(patch));
        completion::completeSection(sendOverlap, recvOverlap, coneSizeSection, coneSection, sendSection, recvSection);
        // Update cones
        updateSieve(recvSection, topology);
      };
      #undef __FUNCT__
      #define __FUNCT__ "createPartitionOverlap"
      static void createPartitionOverlap(const Obj<topology_type>& topology, const Obj<send_overlap_type>& sendOverlap, const Obj<recv_overlap_type>& recvOverlap) {
        const Obj<send_overlap_type>& topSendOverlap = topology->getSendOverlap();
        const Obj<recv_overlap_type>& topRecvOverlap = topology->getRecvOverlap();
        const Obj<typename send_overlap_type::traits::baseSequence> base = topSendOverlap->base();
        const Obj<typename recv_overlap_type::traits::capSequence>  cap  = topRecvOverlap->cap();

        if (base->empty()) {
          if (topology->commRank() == 0) {
            for(int p = 1; p < topology->commSize(); p++) {
              // The arrow is from local partition point p (source) to remote partition point p (color) on rank p (target)
              sendOverlap->addCone(p, p, p);
            }
          }
        } else {
          for(typename send_overlap_type::traits::baseSequence::iterator b_iter = base->begin(); b_iter != base->end(); ++b_iter) {
            const int& p = *b_iter;
            // The arrow is from local partition point p (source) to remote partition point p (color) on rank p (target)
            sendOverlap->addCone(p, p, p);
          }
        }
        if (cap->empty()) {
          if (topology->commRank() != 0) {
            // The arrow is from local partition point rank (color) on rank 0 (source) to remote partition point rank (target)
            recvOverlap->addCone(0, topology->commRank(), topology->commRank());
          }
        } else {
          for(typename recv_overlap_type::traits::capSequence::iterator c_iter = cap->begin(); c_iter != cap->end(); ++c_iter) {
            const int& p = *c_iter;
            // The arrow is from local partition point rank (color) on rank p (source) to remote partition point rank (target)
            recvOverlap->addCone(p, topology->commRank(), topology->commRank());
          }
        }
        if (topology->debug()) {
          sendOverlap->view("Initial send overlap");
          recvOverlap->view("Initial receive overlap");
        }
      }
      #undef __FUNCT__
      #define __FUNCT__ "createAssignment"
      template<typename Partitioner>
      static typename Partitioner::part_type *createAssignment(const Obj<topology_type>& topology, const int dim, const Obj<send_overlap_type>& sendOverlap, const Obj<recv_overlap_type>& recvOverlap) {
        // 1) Form partition point overlap a priori
        createPartitionOverlap(topology, sendOverlap, recvOverlap);
        if (topology->debug()) {
          sendOverlap->view("Send overlap for partition");
          recvOverlap->view("Receive overlap for partition");
        }
        // 2) Partition the mesh
        return Partitioner::partitionSieve(topology, dim);
      };
      #undef __FUNCT__
      #define __FUNCT__ "scatterTopology"
      // Partition a topology on process 0 and scatter to all processes
      static void scatterTopology(const Obj<topology_type>& topology, const int dim, const Obj<topology_type>& topologyNew, const Obj<send_overlap_type>& sendOverlap, const Obj<recv_overlap_type>& recvOverlap, const std::string& partitioner, const Obj<topology_type>& subTopology = NULL, const Obj<topology_type>& subTopologyNew = NULL) {
        if (partitioner == "chaco") {
#ifdef PETSC_HAVE_CHACO
          typedef typename ALECompat::New::Chaco::Partitioner<topology_type> Partitioner;
          typedef typename ALECompat::New::Partitioner<topology_type>        GenPartitioner;
          typedef typename Partitioner::part_type                      part_type;

          part_type *assignment = scatterTopology<Partitioner>(topology, dim, topologyNew, sendOverlap, recvOverlap);
          if (!subTopology.isNull() && !subTopologyNew.isNull()) {
            part_type *subAssignment = GenPartitioner::subordinatePartition(topology, 1, subTopology, assignment);
            Obj<send_overlap_type> subSendOverlap = new send_overlap_type(topology->comm(), topology->debug());
            Obj<recv_overlap_type> subRecvOverlap = new recv_overlap_type(topology->comm(), topology->debug());
            const typename topology_type::patch_type patch      = 0;
            const Obj<sieve_type>&                   sieve      = subTopology->getPatch(patch);
            const Obj<sieve_type>&                   sieveNew   = new Mesh::sieve_type(subTopology->comm(), subTopology->debug());
            const int                                numCells   = subTopology->heightStratum(patch, 0)->size();

            createPartitionOverlap(subTopology, subSendOverlap, subRecvOverlap);
            subTopologyNew->setPatch(0, sieveNew);
            sieveCompletion::scatterSieve(subTopology, sieve, dim, sieveNew, subSendOverlap, subRecvOverlap, numCells, subAssignment);
            subTopologyNew->stratify();
            subTopologyNew->setDistSendOverlap(subSendOverlap);
            subTopologyNew->setDistRecvOverlap(subRecvOverlap);
            if (subAssignment != NULL) delete [] subAssignment;
          }
          if (assignment != NULL) delete [] assignment;
#else
          throw ALE::Exception("Chaco is not installed. Reconfigure with the flag --download-chaco");
#endif
        } else if (partitioner == "parmetis") {
#ifdef PETSC_HAVE_PARMETIS
          typedef typename ALECompat::New::ParMetis::Partitioner<topology_type> Partitioner;
          typedef typename Partitioner::part_type                         part_type;

          part_type *assignment = scatterTopology<Partitioner>(topology, dim, topologyNew, sendOverlap, recvOverlap);
          if (assignment != NULL) delete [] assignment;
#else
          throw ALE::Exception("ParMetis is not installed. Reconfigure with the flag --download-parmetis");
#endif
        } else {
          throw ALE::Exception("Unknown partitioner");
        }
      };
      template<typename Partitioner>
      static typename Partitioner::part_type *scatterTopology(const Obj<topology_type>& topology, const int dim, const Obj<topology_type>& topologyNew, const Obj<send_overlap_type>& sendOverlap, const Obj<recv_overlap_type>& recvOverlap) {
        typename Partitioner::part_type         *assignment = createAssignment<Partitioner>(topology, dim, sendOverlap, recvOverlap);
        const typename topology_type::patch_type patch      = 0;
        const Obj<sieve_type>&                   sieve      = topology->getPatch(patch);
        const Obj<sieve_type>&                   sieveNew   = topologyNew->getPatch(patch);
        const int                                numCells   = topology->heightStratum(patch, 0)->size();

        sieveCompletion::scatterSieve(topology, sieve, dim, sieveNew, sendOverlap, recvOverlap, numCells, assignment);
        topologyNew->stratify();
        return assignment;
      };
      #undef __FUNCT__
      #define __FUNCT__ "distributeMesh"
      static Obj<Mesh> distributeMesh(const Obj<Mesh>& serialMesh, const std::string& partitioner = "chaco") {
        Obj<Mesh> parallelMesh = new Mesh(serialMesh->comm(), serialMesh->getDimension(), serialMesh->debug());
        const Obj<Mesh::topology_type>& serialTopology   = serialMesh->getTopology();
        const Obj<Mesh::topology_type>& parallelTopology = new Mesh::topology_type(serialMesh->comm(), serialMesh->debug());
        const Obj<Mesh::topology_type>& tractionTopology = new Mesh::topology_type(serialMesh->comm(), serialMesh->debug());
        const Obj<Mesh::sieve_type>&    sieve            = new Mesh::sieve_type(serialMesh->comm(), serialMesh->debug());
        const int                       dim              = serialMesh->getDimension();

        //if (serialMesh->getDistributed()) return serialMesh;
        ALE_LOG_EVENT_BEGIN;
        parallelTopology->setPatch(0, sieve);
        parallelMesh->setTopology(parallelTopology);
        if (serialMesh->debug()) {
          serialMesh->view("Serial topology");
        }

        // Distribute cones
        Obj<send_overlap_type> sendOverlap = new send_overlap_type(serialTopology->comm(), serialTopology->debug());
        Obj<recv_overlap_type> recvOverlap = new recv_overlap_type(serialTopology->comm(), serialTopology->debug());
        if (serialMesh->hasRealSection("traction")) {
          const Obj<Mesh::real_section_type>& traction = serialMesh->getRealSection("traction");

          scatterTopology(serialTopology, dim, parallelTopology, sendOverlap, recvOverlap, partitioner, traction->getTopology(), tractionTopology);
        } else {
          scatterTopology(serialTopology, dim, parallelTopology, sendOverlap, recvOverlap, partitioner);
        }
        parallelTopology->setDistSendOverlap(sendOverlap);
        parallelTopology->setDistRecvOverlap(recvOverlap);

        // Distribute labels
        const typename topology_type::labels_type& labels = serialTopology->getLabels();

        for(typename topology_type::labels_type::const_iterator l_iter = labels.begin(); l_iter != labels.end(); ++l_iter) {
          for(typename topology_type::label_type::const_iterator pl_iter = l_iter->second.begin(); pl_iter != l_iter->second.end(); ++pl_iter) {
            if (parallelTopology->hasLabel(l_iter->first, pl_iter->first)) continue;
            const Obj<typename topology_type::patch_label_type>& serialLabel   = pl_iter->second;
            const Obj<typename topology_type::patch_label_type>& parallelLabel = parallelTopology->createLabel(pl_iter->first, l_iter->first);
            // Create local label
            const Obj<typename topology_type::patch_label_type::traits::baseSequence>& base = serialLabel->base();
            const Obj<typename topology_type::sieve_type>& parallelSieve = parallelTopology->getPatch(pl_iter->first);

            for(typename topology_type::patch_label_type::traits::baseSequence::iterator b_iter = base->begin(); b_iter != base->end(); ++b_iter) {
              if (parallelSieve->capContains(*b_iter) || parallelSieve->baseContains(*b_iter)) {
                parallelLabel->addArrow(*serialLabel->cone(*b_iter)->begin(), *b_iter);
              }
            }
            // Get remote labels
            sieveCompletion::scatterCones(serialLabel, parallelLabel, sendOverlap, recvOverlap);
          }
        }

        // Distribute sections
        Obj<std::set<std::string> > sections = serialMesh->getRealSections();

        for(std::set<std::string>::iterator name = sections->begin(); name != sections->end(); ++name) {
          if (*name == "traction") {
            parallelMesh->setRealSection(*name, distributeSection(serialMesh->getRealSection(*name), tractionTopology, sendOverlap, recvOverlap));
          } else {
            parallelMesh->setRealSection(*name, distributeSection(serialMesh->getRealSection(*name), parallelMesh->getTopology(), sendOverlap, recvOverlap));
          }
        }
        sections = serialMesh->getIntSections();
        for(std::set<std::string>::iterator name = sections->begin(); name != sections->end(); ++name) {
          parallelMesh->setIntSection(*name, distributeSection(serialMesh->getIntSection(*name), parallelMesh->getTopology(), sendOverlap, recvOverlap));
        }
        sections = serialMesh->getPairSections();
        for(std::set<std::string>::iterator name = sections->begin(); name != sections->end(); ++name) {
          parallelMesh->setPairSection(*name, distributeSection(serialMesh->getPairSection(*name), parallelMesh->getTopology(), sendOverlap, recvOverlap));
        }

        // This is necessary since we create types (like PartitionSection) on a subset of processors
        if (parallelMesh->debug()) {parallelMesh->view("Parallel Mesh");}
        parallelMesh->setDistributed(true);
        ALE_LOG_EVENT_END;
        return parallelMesh;
      };
      #undef __FUNCT__
      #define __FUNCT__ "distributeSection"
      template<typename Section>
      static Obj<Section> distributeSection(const Obj<Section>& serialSection, const Obj<Mesh::topology_type>& parallelTopology, const Obj<send_overlap_type>& sendOverlap, const Obj<recv_overlap_type>& recvOverlap) {
        if (serialSection->debug()) {
          serialSection->view("Serial Section");
        }
        typedef OverlapValues<send_overlap_type, typename sieveCompletion::topology_type, typename Section::value_type> send_section_type;
        typedef OverlapValues<recv_overlap_type, typename sieveCompletion::topology_type, typename Section::value_type> recv_section_type;
        typedef SizeSection<Section>      SectionSizer;
        // TEST THIS! I think this is unnecessary
        typedef PatchlessSection<Section> SectionFiller;
        Obj<Section>                       parallelSection = new Section(parallelTopology);
        const typename Section::patch_type patch           = 0;
        const Obj<send_section_type>       sendSection     = new send_section_type(serialSection->comm(), serialSection->debug());
        const Obj<recv_section_type>       recvSection     = new recv_section_type(serialSection->comm(), sendSection->getTag(), serialSection->debug());
        const Obj<SectionSizer>            sizer           = new SectionSizer(serialSection, patch);
        const Obj<SectionFiller>           filler          = new SectionFiller(serialSection, patch);

        updateSectionLocal(serialSection, parallelSection);
        sectionCompletion::completeSection(sendOverlap, recvOverlap, sizer, filler, sendSection, recvSection);
        updateSectionRemote(recvOverlap, recvSection, parallelSection);
        if (parallelSection->debug()) {
          parallelSection->view("Parallel Section");
        }
        return parallelSection;
      };
      #undef __FUNCT__
      #define __FUNCT__ "completeSection"
      template<typename Section>
      static void completeSection(const Obj<Section>& section) {
        typedef typename Section::topology_type                       topology_type;
        typedef typename Distribution<topology_type>::sieveCompletion sieveCompletion;
        typedef typename topology_type::send_overlap_type             send_overlap_type;
        typedef typename topology_type::recv_overlap_type             recv_overlap_type;
        typedef typename Section::value_type                          value_type;
        typedef OverlapValues<send_overlap_type, typename sieveCompletion::topology_type, value_type> send_section_type;
        typedef OverlapValues<recv_overlap_type, typename sieveCompletion::topology_type, value_type> recv_section_type;
        typedef SizeSection<Section>      SectionSizer;
        typedef PatchlessSection<Section> SectionFiller;
        const Obj<topology_type>&                topology = section->getTopology();
        const typename topology_type::patch_type patch    = 0;
        const int                                debug    = section->debug();
        topology->constructOverlap(patch);

        const Obj<send_overlap_type> sendOverlap = topology->getSendOverlap();
        const Obj<recv_overlap_type> recvOverlap = topology->getRecvOverlap();
        const Obj<send_section_type> sendSection = new send_section_type(section->comm(), section->debug());
        const Obj<recv_section_type> recvSection = new recv_section_type(section->comm(), sendSection->getTag(), section->debug());
        const Obj<SectionSizer>      sizer       = new SectionSizer(section, patch);
        const Obj<SectionFiller>     filler      = new SectionFiller(section, patch);

        sectionCompletion::completeSection(sendOverlap, recvOverlap, sizer, filler, sendSection, recvSection);
        // Update section with remote data
        const Obj<typename recv_overlap_type::traits::baseSequence> recvPoints = topology->getRecvOverlap()->base();

        for(typename recv_overlap_type::traits::baseSequence::iterator r_iter = recvPoints->begin(); r_iter != recvPoints->end(); ++r_iter) {
          const Obj<typename recv_overlap_type::traits::coneSequence>&     recvPatches = recvOverlap->cone(*r_iter);
          const typename recv_overlap_type::traits::coneSequence::iterator end         = recvPatches->end();

          for(typename recv_overlap_type::traits::coneSequence::iterator p_iter = recvPatches->begin(); p_iter != end; ++p_iter) {
            if (recvSection->getFiberDimension(*p_iter, p_iter.color())) {
              if (debug) {std::cout << "["<<section->commRank()<<"]Completed point " << *r_iter << std::endl;}
              section->updateAddPoint(patch, *r_iter, recvSection->restrictPoint(*p_iter, p_iter.color()));
            }
          }
        }
      };
    };
  }
}
#endif

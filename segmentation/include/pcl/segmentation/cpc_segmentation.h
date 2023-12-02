/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2014-, Open Perception, Inc.
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the copyright holder(s) nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */

#pragma once

// common includes
#include <pcl/pcl_base.h>
#include <pcl/point_types.h>
#include <pcl/point_cloud.h>

// segmentation and sample consensus includes
#include <pcl/segmentation/supervoxel_clustering.h>
#include <pcl/segmentation/lccp_segmentation.h>
#include <pcl/sample_consensus/sac.h>

#include <pcl/segmentation/extract_clusters.h>

#define PCL_INSTANTIATE_CPCSegmentation(T) template class PCL_EXPORTS pcl::CPCSegmentation<T>;

namespace pcl
{
  /** \brief A segmentation algorithm partitioning a supervoxel graph. It uses planar cuts induced by local concavities for the recursive segmentation. Cuts are estimated using locally constrained directed RANSAC.
   *  \note If you use this in a scientific work please cite the following paper:
   *  M. Schoeler, J. Papon, F. Woergoetter
   *  Constrained Planar Cuts - Object Partitioning for Point Clouds
   *  In Proceedings of the IEEE Conference on Computer Vision and Pattern Recognition (CVPR) 2015
   *  Inherits most of its functionality from \ref LCCPSegmentation
   *  \author Markus Schoeler (mschoeler@web.de)
   *  \ingroup segmentation
   */
  template <typename PointT>
  class CPCSegmentation : public LCCPSegmentation<PointT>
  {
      using WeightSACPointType = PointXYZINormal;
      using LCCP = LCCPSegmentation<PointT>;
      // LCCP typedefs
      using EdgeID = typename LCCP::EdgeID;
      using EdgeIterator = typename LCCP::EdgeIterator;
      // LCCP methods
      using LCCP::calculateConvexConnections;
      using LCCP::applyKconvexity;
      using LCCP::doGrouping;
      using LCCP::mergeSmallSegments;
      // LCCP variables
      using LCCP::sv_adjacency_list_;
      using LCCP::k_factor_;
      using LCCP::grouping_data_valid_;
      using LCCP::sv_label_to_seg_label_map_;
      using LCCP::sv_label_to_supervoxel_map_;
      using LCCP::concavity_tolerance_threshold_;
      using LCCP::seed_resolution_;
      using LCCP::supervoxels_set_;

    public:
      CPCSegmentation ();

      ~CPCSegmentation () override;

      /** \brief Merge supervoxels using cuts through local convexities. The input parameters are generated by using the \ref SupervoxelClustering class. To retrieve the output use the \ref relabelCloud method.
       *  \note There are three ways to retrieve the segmentation afterwards (inherited from \ref LCCPSegmentation): \ref relabelCloud, \ref getSupervoxelToSegmentMap and \ref getSupervoxelToSegmentMap */
      void
      segment ();

      /** \brief Determines if we want to use cutting planes
       *  \param[in] max_cuts Maximum number of cuts
       *  \param[in] cutting_min_segments Minimum segment size for cutting
       *  \param[in] cutting_min_score Minimum score a proposed cut has to achieve for being performed
       *  \param[in] locally_constrained Decide if we constrain our cuts locally
       *  \param[in] directed_cutting Decide if we prefer cuts perpendicular to the edge-direction
       *  \param[in] clean_cutting Decide if we cut only edges with supervoxels on opposite sides of the plane (clean) or all edges within the seed_resolution_ distance to the plane (not clean). The later was used in the paper.
       */
      inline void
      setCutting (const std::uint32_t max_cuts = 20,
                  const std::uint32_t cutting_min_segments = 0,
                  const float cutting_min_score = 0.16,
                  const bool locally_constrained = true,
                  const bool directed_cutting = true,
                  const bool clean_cutting = false)
      {
        max_cuts_ = max_cuts;
        min_segment_size_for_cutting_ = cutting_min_segments;
        min_cut_score_ = cutting_min_score;
        use_local_constrains_ = locally_constrained;
        use_directed_weights_ = directed_cutting;
        use_clean_cutting_ = clean_cutting;
      }

      /** \brief Set the number of iterations for the weighted RANSAC step (best cut estimations)
       *  \param[in] ransac_iterations The number of iterations */
      inline void
      setRANSACIterations (const std::uint32_t ransac_iterations)
      {
        ransac_itrs_ = ransac_iterations;
      }

    private:

      /** \brief Used in for CPC to find and fit cutting planes to the pointcloud.
       *  \note Is used recursively
       *  \param[in] depth_levels_left When first calling the function set this parameter to the maximum levels you want to cut down */
      void
      applyCuttingPlane (std::uint32_t depth_levels_left);

      ///  *** Parameters *** ///

      /** \brief Maximum number of cuts */
      std::uint32_t max_cuts_{20};

      /** \brief Minimum segment size for cutting */
      std::uint32_t min_segment_size_for_cutting_{400};

      /** \brief Cut_score threshold */
      float min_cut_score_{0.16};

      /** \brief Use local constrains for cutting */
      bool use_local_constrains_{true};

      /** \brief Use directed weights for the cutting */
      bool use_directed_weights_{true};

      /** \brief Use clean cutting */
      bool use_clean_cutting_{false};

      /** \brief Iterations for RANSAC */
      std::uint32_t ransac_itrs_{10000};


/******************************************* Directional weighted RANSAC declarations ******************************************************************/
      /** \brief @b WeightedRandomSampleConsensus represents an implementation of the Directionally Weighted RANSAC algorithm, as described in: "Constrained Planar Cuts - Part Segmentation for Point Clouds", CVPR 2015, M. Schoeler, J. Papon, F. Wörgötter.
        *  \note It only uses points with a weight > 0 for the model calculation, but uses all points for the evaluation (scoring of the model)
        *  Only use in conjunction with sac_model_plane
        *  If you use this in a scientific work please cite the following paper:
        *  M. Schoeler, J. Papon, F. Woergoetter
        *  Constrained Planar Cuts - Object Partitioning for Point Clouds
        *  In Proceedings of the IEEE Conference on Computer Vision and Pattern Recognition (CVPR) 2015
        *  \author Markus Schoeler (mschoeler@web.de)
        *  \ingroup segmentation
        */

      class WeightedRandomSampleConsensus : public SampleConsensus<WeightSACPointType>
      {
          using SampleConsensusModelPtr = SampleConsensusModel<WeightSACPointType>::Ptr;

        public:
          using Ptr = shared_ptr<WeightedRandomSampleConsensus>;
          using ConstPtr = shared_ptr<const WeightedRandomSampleConsensus>;

          /** \brief WeightedRandomSampleConsensus (Weighted RAndom SAmple Consensus) main constructor
            * \param[in] model a Sample Consensus model
            * \param[in] random if true set the random seed to the current time, else set to 12345 (default: false)
            */
          WeightedRandomSampleConsensus (const SampleConsensusModelPtr &model,
                                        bool random = false)
            : SampleConsensus<WeightSACPointType> (model, random)
          {
            initialize ();
          }

          /** \brief WeightedRandomSampleConsensus (Weighted RAndom SAmple Consensus) main constructor
            * \param[in] model a Sample Consensus model
            * \param[in] threshold distance to model threshold
            * \param[in] random if true set the random seed to the current time, else set to 12345 (default: false)
            */
          WeightedRandomSampleConsensus (const SampleConsensusModelPtr &model,
                                        double threshold,
                                        bool random = false)
            : SampleConsensus<WeightSACPointType> (model, threshold, random)
          {
            initialize ();
          }

          /** \brief Compute the actual model and find the inliers
            * \param[in] debug_verbosity_level enable/disable on-screen debug information and set the verbosity level
            */
          bool
          computeModel (int debug_verbosity_level = 0) override;

          /** \brief Set the weights for the input points
          *  \param[in] weights Weights for input samples. Negative weights are counted as penalty.
          */
          void
          setWeights (const std::vector<double> &weights,
                      const bool directed_weights = false)
          {
            if (weights.size () != full_cloud_pt_indices_->size ())
            {
              PCL_ERROR ("[pcl::WeightedRandomSampleConsensus::setWeights] Cannot assign weights. Weight vector needs to have the same length as the input pointcloud\n");
              return;
            }
            weights_ = weights;
            model_pt_indices_->clear ();
            for (std::size_t i = 0; i < weights.size (); ++i)
            {
              if (weights[i] > std::numeric_limits<double>::epsilon ())
                model_pt_indices_->push_back (i);
            }
            use_directed_weights_ = directed_weights;
          }

          /** \brief Get the best score
          *  \returns The best score found.
          */
          double
          getBestScore () const
          {
            return (best_score_);
          }

        protected:
          /** \brief Initialize the model parameters. Called by the constructors. */
          void
          initialize ()
          {
            // Maximum number of trials before we give up.
            max_iterations_ = 10000;
            use_directed_weights_ = false;
            model_pt_indices_.reset (new Indices);
            full_cloud_pt_indices_.reset (new Indices (* (sac_model_->getIndices ())));
            point_cloud_ptr_ = sac_model_->getInputCloud ();
          }

          /** \brief  weight each positive weight point by the inner product between the normal and the plane normal */
          bool use_directed_weights_;

          /** \brief  vector of weights assigned to points. Set by the setWeights-method */
          std::vector<double> weights_;

          /** \brief  The indices used for estimating the RANSAC model. Only those whose weight is > 0 */
          pcl::IndicesPtr model_pt_indices_;

          /** \brief  The complete list of indices used for the model evaluation */
          pcl::IndicesPtr full_cloud_pt_indices_;

          /** \brief  Pointer to the input PointCloud */
          pcl::PointCloud<WeightSACPointType>::ConstPtr point_cloud_ptr_;

          /** \brief  Highest score found so far */
          double best_score_;
      };

  };
}

#ifdef PCL_NO_PRECOMPILE
  #include <pcl/segmentation/impl/cpc_segmentation.hpp>
#elif defined(PCL_ONLY_CORE_POINT_TYPES)
  //pcl::PointXYZINormal is not a core point type (so we cannot use the precompiled classes here)
  #include <pcl/sample_consensus/impl/sac_model_plane.hpp>
  #include <pcl/segmentation/impl/extract_clusters.hpp>
#endif // PCL_NO_PRECOMPILE / PCL_ONLY_CORE_POINT_TYPES

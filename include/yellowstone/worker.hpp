/**
 * @file worker.hpp
 * @brief Documents Yellowstone's Phelps worker integration seam.
 *
 * EventBus owns its consumer Worker implementation internally. Application code
 * should subscribe handlers rather than constructing worker threads directly.
 */
#pragma once

#include "phelps/manager.hpp"

namespace yellowstone
{
  using Worker = phelps::WorkerManager::Worker;
  using WorkerPtr = phelps::WorkerManager::WorkerPtr;
} // namespace yellowstone

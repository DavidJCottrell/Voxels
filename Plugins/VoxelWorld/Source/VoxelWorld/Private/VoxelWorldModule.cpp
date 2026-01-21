// Copyright Your Company. All Rights Reserved.

#include "VoxelWorldModule.h"

DEFINE_LOG_CATEGORY(LogVoxelWorld);

#define LOCTEXT_NAMESPACE "FVoxelWorldModule"

void FVoxelWorldModule::StartupModule()
{
    UE_LOG(LogVoxelWorld, Log, TEXT("VoxelWorld module has started."));
}

void FVoxelWorldModule::ShutdownModule()
{
    UE_LOG(LogVoxelWorld, Log, TEXT("VoxelWorld module has shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FVoxelWorldModule, VoxelWorld)

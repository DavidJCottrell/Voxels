// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVoxelWorld, Log, All);

class FVoxelWorldModule : public IModuleInterface
{
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    /**
     * Singleton-like access to this module's interface.
     *
     * @return Returns singleton instance, loading the module on demand if needed
     */
    static inline FVoxelWorldModule& Get()
    {
        return FModuleManager::LoadModuleChecked<FVoxelWorldModule>("VoxelWorld");
    }

    /**
     * Checks to see if this module is loaded and ready.
     *
     * @return True if the module is loaded and ready to use
     */
    static inline bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("VoxelWorld");
    }
};

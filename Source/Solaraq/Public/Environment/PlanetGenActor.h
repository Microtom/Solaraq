// Environment/PlanetGenActor.h

#pragma once

#include "CoreMinimal.h"
#include "Environment/CelestialBodyBase.h"
#include "PlanetGenActor.generated.h"

/**
 * Struct to hold visual configuration. 
 * This makes it easy to copy/paste settings between planets or save to DataTables later.
 */
USTRUCT(BlueprintType)
struct FPlanetVisualSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Colors")
    FLinearColor DeepOceanColor = FLinearColor(0.02f, 0.05f, 0.2f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Colors")
    FLinearColor ShallowWaterColor = FLinearColor(0.1f, 0.4f, 0.8f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Colors")
    FLinearColor LandColorA = FLinearColor(0.1f, 0.3f, 0.05f, 1.0f); // Biome 1

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Colors")
    FLinearColor LandColorB = FLinearColor(0.4f, 0.3f, 0.15f, 1.0f); // Biome 2

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Atmosphere")
    FLinearColor AtmosphereColor = FLinearColor(0.6f, 0.8f, 1.0f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Atmosphere", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CloudDensity = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Terrain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float WaterLevel = 0.5f; // Determines ocean vs land ratio via noise threshold

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Terrain")
    float IceCapLevel = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Glow", meta = (ClampMin = "0.0"))
    float CityLightsIntensity = 0.0f; // For populated planets

    /** If true, disables water and clouds, enables craters */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Type")
    bool bIsBarren = false;

    /** The Normal Map texture containing the crater stamps */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Surface", meta = (EditCondition = "bIsBarren"))
    UTexture* CraterNormalTexture;

    /** How large the craters are */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Surface", meta = (EditCondition = "bIsBarren"))
    float CraterScale = 0.001f;

    /** Strength of the crater depth effect */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Surface", meta = (EditCondition = "bIsBarren"))
    float CraterIntensity = 1.0f;
};

UCLASS()
class SOLARAQ_API ASolaraqPlanet : public ACelestialBodyBase
{
    GENERATED_BODY()

public:
    ASolaraqPlanet();

    virtual void OnConstruction(const FTransform& Transform) override;
    
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
    // --- Components ---
    
    /** The opaque sphere for ground/ocean */
    // (BodyMeshComponent is already inherited from Base)

    /** The slightly larger translucent sphere for clouds/atmosphere */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* AtmosphereMeshComponent;
    
    // --- Properties ---

    /** The master material that supports procedural noise/coloring */
    UPROPERTY(EditDefaultsOnly, Category = "Planet Visuals")
    UMaterialInterface* PlanetMasterMaterial;

    /** Material for the atmosphere shell (Needs to be Translucent) */
    UPROPERTY(EditDefaultsOnly, Category = "Planet Visuals")
    UMaterialInterface* AtmosphereMasterMaterial;

    // DMI for the atmosphere
    UPROPERTY(Transient)
    UMaterialInstanceDynamic* AtmosphereMaterialInstance;
    
    /** The actual settings for this specific instance */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Visuals")
    FPlanetVisualSettings VisualSettings;

    /** Seed for procedural noise generation (change this to change land shapes) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Visuals")
    float PlanetSeed;

    /** Editor tool: Click to randomize visual settings */
    UPROPERTY(EditAnywhere, Category = "Planet Maker Tool")
    bool bRandomizeVisuals;

    // --- Internal ---
    
    // The dynamic material instance we write to
    UPROPERTY(Transient)
    UMaterialInstanceDynamic* PlanetMaterialInstance;

    void UpdatePlanetMaterial();
    void RandomizePlanet();
};
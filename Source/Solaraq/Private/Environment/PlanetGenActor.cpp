// Environment/PlanetGenActor.cpp

#include "Environment/PlanetGenActor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"

ASolaraqPlanet::ASolaraqPlanet()
{
    // Default Seed
    PlanetSeed = 1234.5f;
    bRandomizeVisuals = false;

    AtmosphereMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AtmosphereMesh"));
    AtmosphereMeshComponent->SetupAttachment(SceneRoot);
    
    // Key Settings:
    AtmosphereMeshComponent->SetCollisionProfileName(FName("NoCollision")); // Don't block ships or lasers
    AtmosphereMeshComponent->SetCastShadow(false); // Atmospheres usually don't cast hard shadows
    
    // Scale it up slightly so it hovers over the planet
    // 1.05 means the atmosphere is 5% taller than the ground
    AtmosphereMeshComponent->SetRelativeScale3D(FVector(1.05f, 1.05f, 1.05f)); 
}

void ASolaraqPlanet::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform); // This calls ValidateRadii from the base class

    UpdatePlanetMaterial();
}

#if WITH_EDITOR
void ASolaraqPlanet::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

    // If the user clicked the "Randomize" bool
    if (PropertyName == GET_MEMBER_NAME_CHECKED(ASolaraqPlanet, bRandomizeVisuals))
    {
        bRandomizeVisuals = false; // Reset toggle
        RandomizePlanet();
        UpdatePlanetMaterial();
    }
    // If any visual settings changed, update the material immediately
    else if (PropertyChangedEvent.Property->GetOwnerStruct() == FPlanetVisualSettings::StaticStruct() ||
             PropertyName == GET_MEMBER_NAME_CHECKED(ASolaraqPlanet, PlanetSeed) ||
             PropertyName == GET_MEMBER_NAME_CHECKED(ASolaraqPlanet, PlanetMasterMaterial))
    {
        UpdatePlanetMaterial();
    }
}
#endif

void ASolaraqPlanet::UpdatePlanetMaterial()
{
    // ==============================================================================
    // 1. Update the Planet Surface (Opaque Layer: Land, Ocean, City Lights)
    // ==============================================================================
    if (BodyMeshComponent)
    {
        // Safety: Ensure we have a Master Material to generate from
        if (!PlanetMasterMaterial)
        {
            PlanetMasterMaterial = BodyMeshComponent->GetMaterial(0);
        }

        if (PlanetMasterMaterial)
        {
            // Create the Dynamic Material Instance (DMI) if it doesn't exist,
            // or recreate it if the Master Material was swapped in the editor.
            if (!PlanetMaterialInstance || PlanetMaterialInstance->Parent != PlanetMasterMaterial)
            {
                PlanetMaterialInstance = UMaterialInstanceDynamic::Create(PlanetMasterMaterial, this);
                BodyMeshComponent->SetMaterial(0, PlanetMaterialInstance);
            }

            // Push properties to the Shader
            if (PlanetMaterialInstance)
            {
                // --- Colors ---
                PlanetMaterialInstance->SetVectorParameterValue(FName("DeepOceanColor"), VisualSettings.DeepOceanColor);
                PlanetMaterialInstance->SetVectorParameterValue(FName("ShallowWaterColor"), VisualSettings.ShallowWaterColor);
                PlanetMaterialInstance->SetVectorParameterValue(FName("LandColorA"), VisualSettings.LandColorA);
                PlanetMaterialInstance->SetVectorParameterValue(FName("LandColorB"), VisualSettings.LandColorB);
                
                // Pass Atmosphere color to the ground too. 
                // Why? To tint the ground at the edges (horizon) so it matches the sky color.
                PlanetMaterialInstance->SetVectorParameterValue(FName("AtmosphereColor"), VisualSettings.AtmosphereColor);

                // --- Terrain Logic ---
                PlanetMaterialInstance->SetScalarParameterValue(FName("WaterLevel"), VisualSettings.WaterLevel);
                PlanetMaterialInstance->SetScalarParameterValue(FName("IceCapLevel"), VisualSettings.IceCapLevel);
                PlanetMaterialInstance->SetScalarParameterValue(FName("CityLightsIntensity"), VisualSettings.CityLightsIntensity);
                
                // --- Generation Seed ---
                PlanetMaterialInstance->SetScalarParameterValue(FName("SeedOffset"), PlanetSeed);
            }
        }
    }

    // ==============================================================================
    // 2. Update the Atmosphere Shell (Translucent Layer: Clouds, Haze)
    // ==============================================================================
    if (AtmosphereMeshComponent)
    {
        // Safety: Ensure we have a Master Material
        if (!AtmosphereMasterMaterial)
        {
            AtmosphereMasterMaterial = AtmosphereMeshComponent->GetMaterial(0);
        }

        if (AtmosphereMasterMaterial)
        {
            // Create the DMI for the atmosphere
            if (!AtmosphereMaterialInstance || AtmosphereMaterialInstance->Parent != AtmosphereMasterMaterial)
            {
                AtmosphereMaterialInstance = UMaterialInstanceDynamic::Create(AtmosphereMasterMaterial, this);
                AtmosphereMeshComponent->SetMaterial(0, AtmosphereMaterialInstance);
            }

            // Push properties to the Shader
            if (AtmosphereMaterialInstance)
            {
                // --- Visuals ---
                AtmosphereMaterialInstance->SetVectorParameterValue(FName("AtmosphereColor"), VisualSettings.AtmosphereColor);
                AtmosphereMaterialInstance->SetScalarParameterValue(FName("CloudDensity"), VisualSettings.CloudDensity);

                // --- Generation Seed ---
                // We add an arbitrary offset (e.g., +531.0f) to the seed for the clouds.
                // This ensures the Cloud Noise pattern is different from the Land Noise pattern.
                // If we didn't do this, clouds would look like they were stamped exactly over the continents.
                AtmosphereMaterialInstance->SetScalarParameterValue(FName("SeedOffset"), PlanetSeed + 531.0f);
            }
        }
    }
}

void ASolaraqPlanet::RandomizePlanet()
{
    // Generate random seed
    PlanetSeed = FMath::RandRange(0.0f, 10000.0f);

    // Randomize properties slightly (optional)
    VisualSettings.WaterLevel = FMath::RandRange(0.3f, 0.7f);
    VisualSettings.CloudDensity = FMath::RandRange(0.2f, 0.8f);
    
    // You could also randomize colors here, but usually, artists prefer manual color control
    // while randomizing the land shapes (Seed).
}
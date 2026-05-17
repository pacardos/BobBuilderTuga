// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BobBuilder.generated.h"



UENUM(BlueprintType)
enum class ETileState : uint8
{
	None,
	Available,
	Selected,
	Loaded
};



USTRUCT(BlueprintType)
struct FChunkHeights
{
	GENERATED_BODY()

	UPROPERTY()
	int32 ChunkIndex = -1;

	UPROPERTY()
	TArray<float> Heights;

	UPROPERTY()
	uint16 HoleMask = 0;
};

USTRUCT(BlueprintType)
struct FChunkNormals
{
	GENERATED_BODY()

	UPROPERTY()
	int32 ChunkIndex = -1;

	UPROPERTY()
	TArray<FVector3f> Normals;
};



UCLASS()
class WOWTUGA_API ABobBuilder : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ABobBuilder();

	UPROPERTY(EditAnywhere, Category = "Bob Builder", meta = (FilePathFilter = "wdt"))
	FFilePath WDTFile;

	UFUNCTION(CallInEditor, Category = "Bob Builder")
	void LoadFiles();

	UPROPERTY()
	TArray<ETileState> TileMap;

	void OpenMapSelector();

	void ToggleTileSelection(int32 Index);

	UFUNCTION(CallInEditor, Category = "Bob Builder")
	void Generate();

	UPROPERTY(EditAnywhere, Category = "Bob Builder")
	bool bImportHeightmap = true;

	UPROPERTY(EditAnywhere, Category = "Bob Builder")
	bool bDoodads = false;

	UPROPERTY(EditAnywhere, Category = "Bob Builder")
	bool bWMOs = false;

	UPROPERTY(EditAnywhere, Category = "Bob Builder")
	bool bTextures = false;




protected:
	// Called when the game starts or when spawned



private:
	UPROPERTY()
	FString mapname;

	UPROPERTY()
	FString txtdir;

	UPROPERTY()
	FString alphadir;

	bool ParseWDT_HeaderOnly(FString FullPath);

	UPROPERTY()
	TSet<FIntPoint> SelectedADTs;

	void BobBuildOrder();

	// NO UPROPERTY for static constexpr
	// as 'static constexpr' it will only be calculated once at start
	static constexpr float TILE = 1600.0f / 3.0f * 100.0f;
	static constexpr float CHUNK = TILE / 16.0f;
	static constexpr float QUAD = CHUNK / 8.0f;
	static constexpr float ORIGIN = TILE * 32.0f;
	static constexpr float WOX = -ORIGIN;
	static constexpr float WOY = ORIGIN;

	void BuildTerrain(int32 TileX, int32 TileY);
	void BuildDoodads(int32 TileX, int32 TileY);
	void BuildWMOs(int32 TileX, int32 TileY);
	void BuildMaterials(int32 TileX, int32 TileY);
};

// Fill out your copyright notice in the Description page of Project Settings.


#include "BobBuilder.h"
#include "UI/SADTGridWidget.h"
// progress bar
#include "Misc/ScopedSlowTask.h"
// create dynamic meshes
#include "DynamicMeshActor.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "EngineUtils.h"
#include "IndexTypes.h"

#define LOCTEXT_NAMESPACE "BobBuilder"



using namespace UE::Geometry;



// Sets default values
ABobBuilder::ABobBuilder()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

}



FString GetTagString(uint32 Tag) {
    ANSICHAR TagStr[5];
    TagStr[0] = (ANSICHAR)(Tag & 0xFF);
    TagStr[1] = (ANSICHAR)((Tag >> 8) & 0xFF);
    TagStr[2] = (ANSICHAR)((Tag >> 16) & 0xFF);
    TagStr[3] = (ANSICHAR)((Tag >> 24) & 0xFF);
    TagStr[4] = '\0';
    return FString(UTF8_TO_TCHAR(TagStr));
}

FString SanitizeWoWPath(const FString& RawPath)
{
    if (RawPath.IsEmpty()) return FString();

    // 1. Trim e Lowercase (Passos 1 e 4 do teu original)
    FString CleanPath = RawPath.TrimStartAndEnd().ToLower();

    // 2. Standardize slashes: \ para / (Passo 2)
    CleanPath = CleanPath.Replace(TEXT("\\"), TEXT("/"));

    // 3. Remover espaços internos (Passo 3)
    CleanPath = CleanPath.Replace(TEXT(" "), TEXT(""));

    // 4. Remover caracteres não imprimíveis (Passo 5)
    // No Unreal, podemos filtrar a string garantindo apenas caracteres válidos
    FString FinalPath = "";
    for (int32 i = 0; i < CleanPath.Len(); ++i)
    {
        TCHAR C = CleanPath[i];
        // Mantém apenas o que for imprimível
        if (FChar::IsPrint(C))
        {
            FinalPath.AppendChar(C);
        }
    }

    return FinalPath;
}



void ABobBuilder::LoadFiles()
{
    TileMap.Init(ETileState::None, 4096);

    FString wdtfullpath = FPaths::ConvertRelativePathToFull(WDTFile.FilePath);

    if (wdtfullpath.IsEmpty() || !FPaths::FileExists(wdtfullpath))
    {
        UE_LOG(LogTemp, Error, TEXT("Bob Builder: WDT file was not selected or not found"));
        return;
    }

	mapname = FPaths::GetBaseFilename(wdtfullpath);
	FString tmpmapdir = FPaths::GetPath(wdtfullpath);

	txtdir = FPaths::Combine(tmpmapdir, FString::Printf(TEXT("_Data")));
	alphadir = FPaths::Combine(txtdir, FString::Printf(TEXT("_splatmap")));

	if (ParseWDT_HeaderOnly(wdtfullpath))
	{
		// CHAMA AUTOMATICAMENTE A JANELA
		OpenMapSelector();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Bob Builder: couldnt find WDT file to read"));
	}
}



bool ABobBuilder::ParseWDT_HeaderOnly(FString FullPath)
{
    TArray<uint8> RawData;
    if (!FFileHelper::LoadFileToArray(RawData, *FullPath)) return false;
    FMemoryReader Reader(RawData);

    while (!Reader.AtEnd()) {
        uint32 Tag, Size;
        Reader << Tag; Reader << Size;
        int64 Next = Reader.Tell() + Size;

        FString TagStr = GetTagString(Tag);

        // MAIN
        if (TagStr == "MAIN" || TagStr == "NIAM") {
            for (int i = 0; i < 4096; ++i) {
                uint32 Flags, Unused;
                Reader << Flags; Reader << Unused;
                if (Flags & 1) {
                    TileMap[i] = ETileState::Available;
                }
            }
        }
        Reader.Seek(Next);
    }
    return true;
}

void ABobBuilder::OpenMapSelector()
{
    TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(FText::FromString("WoW ADT Selector"))
        .ClientSize(FVector2D(640, 640))
        .SupportsMaximize(false)[SNew(SADTGridWidget).OwnerLoader(this)];
    FSlateApplication::Get().AddWindow(Window);
}

void ABobBuilder::ToggleTileSelection(int32 Index) {
    if (TileMap.IsValidIndex(Index)) {
        if (TileMap[Index] == ETileState::Available) TileMap[Index] = ETileState::Selected;
        else if (TileMap[Index] == ETileState::Selected) TileMap[Index] = ETileState::Available;
    }
}

void ABobBuilder::Generate()
{
    SelectedADTs.Empty();

    for (int32 i = 0; i < 4096; ++i)
    {
        if (TileMap[i] == ETileState::Selected)
        {
            int32 TileX = i % 64;
            int32 TileY = i / 64;

            SelectedADTs.Add(FIntPoint(TileX, TileY));
            
            BobBuildOrder();
        }
    }
}



void ABobBuilder::BobBuildOrder()
{
    // get work amount
    int work = 0;
    int subwork = 0;
    int totalwork = 0;
    int workdone = 0;

    work = SelectedADTs.Num();
    
    if (bImportHeightmap) subwork++;
    if (bDoodads) subwork++;
    if (bWMOs) subwork++;
    if (bTextures) subwork++;

    totalwork = (work * subwork);

    FScopedSlowTask BobSlowTask(totalwork, LOCTEXT("BobSlowTask", "Bob is working..."), true);
    BobSlowTask.MakeDialog(true);

    for (const FIntPoint& Tile : SelectedADTs)
    {
        FString adtname = FString::Printf(TEXT("%s_%u_%u"), *mapname, Tile.X, Tile.Y);
        FFormatNamedArguments Args;
        Args.Add(TEXT("AdtName"), FText::FromString(adtname));
        FText StatusMsg = FText::Format(LOCTEXT("ParsingTile", "Terrain: {AdtName}"), Args);

        if (bImportHeightmap)
        {
            float percdone = (workdone / totalwork);
            BobSlowTask.EnterProgressFrame(percdone, StatusMsg);

            BuildTerrain(Tile.X, Tile.Y);

            workdone++;
        }

        if (bDoodads)
        {
            float percdone = (workdone / totalwork);
            BobSlowTask.EnterProgressFrame(percdone, StatusMsg);

            BuildDoodads(Tile.X, Tile.Y);

            workdone++;
        }

        if (bWMOs)
        {
            float percdone = (workdone / totalwork);
            BobSlowTask.EnterProgressFrame(percdone, StatusMsg);

            BuildWMOs(Tile.X, Tile.Y);

            workdone++;
        }

        if (bTextures)
        {
            float percdone = (workdone / totalwork);
            BobSlowTask.EnterProgressFrame(percdone, StatusMsg);

            BuildMaterials(Tile.X, Tile.Y);

            workdone++;
        }
    }
}



void ABobBuilder::BuildTerrain(int32 TileX, int32 TileY)
{
    float adt_pos_x = WOX + (TileY * TILE);
    float adt_pos_y = WOY - (TileX * TILE);

    TArray<FChunkHeights> OutChunkHeights;
    TArray<FChunkNormals> OutChunkNormals;

    FString filename = FString::Printf(
        TEXT("%s_%d_%d.txt"),
        *mapname,
        TileX,
        TileY
    );
    FString txtPath = FPaths::Combine(*txtdir, filename);

    // load file into Lines array
    TArray<FString> Lines;
    if (!FFileHelper::LoadFileToStringArray(Lines, *txtPath)) return;

    // --- PASS 0 - GET HOLES, HEIGHTS AND NORMALS ---
    // get all heights for 256 chunks in a structure -> chunk index | 145 float heights
    // get all normals for 256 chunks in s structure -> chunk index | 145 FVector3f

    // Define a temporary variable outside the loop to store the last seen hole mask
    uint64 LastParsedHoleMask = 0;

    for (const FString& RawLine : Lines)
    {
        FString Line = RawLine.TrimStartAndEnd();

        // get holes
        if (Line.StartsWith(TEXT("HOLE")))
        {
            TArray<FString> SpaceTokens;
            Line.ParseIntoArray(SpaceTokens, TEXT(" "), true);

            if (SpaceTokens.Num() >= 3)
            {
                // The '16' at the end tells Unreal to read it as a Hexadecimal bitmask!
                LastParsedHoleMask = (uint16)FCString::Strtoi(*SpaceTokens[2], nullptr, 16);
            }
            continue;
        }

        // get all heights
        if (Line.StartsWith(TEXT("MCVT")))
        {

            TArray<FString> SpaceTokens;
            Line.ParseIntoArray(SpaceTokens, TEXT(" "), true);

            // Expected signature format: MCVT chunkindex [145 float values...]
            if (SpaceTokens.Num() < 147) continue;

            FChunkHeights Cheight;
            Cheight.ChunkIndex = FCString::Atoi(*SpaceTokens[1]);
            Cheight.Heights.Reserve(145);

            for (int32 i = 2; i < SpaceTokens.Num(); i++)
            {
                Cheight.Heights.Add(FCString::Atof(*SpaceTokens[i]));
            }
            Cheight.HoleMask = LastParsedHoleMask;

            OutChunkHeights.Add(Cheight);
        }

        // get all normals
        if (Line.StartsWith(TEXT("MCNR")))
        {

            TArray<FString> SpaceTokens;
            Line.ParseIntoArray(SpaceTokens, TEXT(" "), true);

            // Expected signature format: MCNR chunkindex [145 FVector3f values...]
            if (SpaceTokens.Num() < 147) continue;

            FChunkNormals Cnormal;
            Cnormal.ChunkIndex = FCString::Atoi(*SpaceTokens[1]);
            Cnormal.Normals.Reserve(145);

            for (int32 i = 2; i < SpaceTokens.Num(); i++)
            {
                // 1. Grab the single token (e.g., "0.12;-0.5;0.88")
                FString NormalToken = SpaceTokens[i];

                // 2. Parse it into sub-tokens using the semicolon as a separator
                TArray<FString> ComponentTokens;
                NormalToken.ParseIntoArray(ComponentTokens, TEXT(";"), true);

                // 3. Ensure we have exactly 3 parts (X, Y, and Z) to prevent a crash
                if (ComponentTokens.Num() == 3)
                {
                    float NX = FCString::Atof(*ComponentTokens[0]);
                    float NY = FCString::Atof(*ComponentTokens[1]);
                    float NZ = FCString::Atof(*ComponentTokens[2]);

                    // 4. Create the vector and add it to our array
                    FVector3f NormalVector(NX, NY, NZ);
                    Cnormal.Normals.Add(NormalVector);
                }
                else
                {
                    // Fallback default up-vector if something went wrong with this specific token
                    Cnormal.Normals.Add(FVector3f(0.0f, 0.0f, 1.0f));
                }
                OutChunkNormals.Add(Cnormal);
            }
        }
    }
  
    for (int r = 0; r < 16; r++)
    {
        for (int c = 0; c < 16; c++)
        {
            UE::Geometry::FDynamicMesh3 Mesh;
            Mesh.EnableAttributes();
            // enable uv attribute
            FDynamicMeshUVOverlay* UVOverlay = Mesh.Attributes()->PrimaryUV();
            TArray<int32> UVHandles;
            // enable normals attribute
            FDynamicMeshNormalOverlay* NormalOverlay = Mesh.Attributes()->PrimaryNormals();
            if (!NormalOverlay)
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to initialize Normal Overlay"));
                return;
            }
            TArray<int32> NormalHandles;

            float chunk_pos_x = adt_pos_x + (r * CHUNK);
            float chunk_pos_y = adt_pos_y - (c * CHUNK);
            int chunkindex = ((r * 16) + c);

            // --- PASS 1: VERTICES ONLY ---
            int vert = 0;

            for (int row = 0; row < 17; row++)
            {
                bool bIsEven = (row % 2 == 0);
                int ncol = bIsEven ? 9 : 8;

                for (int col = 0; col < ncol; ++col) {
                    float px = chunk_pos_x + (row * QUAD * 0.5f);
                    float py = bIsEven ? (chunk_pos_y - (col * QUAD)) : (chunk_pos_y - (col * QUAD) - (QUAD * 0.5f));
                    float pz = 0.0f;

                    if (OutChunkHeights.IsValidIndex(chunkindex))
                    {
                        FChunkHeights& MyChunk = OutChunkHeights[chunkindex];
                        pz = (MyChunk.Heights[vert] * 100.0f);
                    }

                    FVector UEVertex(px, py, pz);
                    Mesh.AppendVertex(FVector3d(UEVertex));

                    vert++;
                }
            }

            // --- PASS 2: GENERATE UV HANDLES ---
            TArray<int32> GlobalUVHandles;
            FAxisAlignedBox3d Bounds = Mesh.GetBounds();
            FVector3d Min = Bounds.Min;
            FVector3d Max = Bounds.Max;
            FVector3d MeshSize = Max - Min;

            double DivX = (MeshSize.X > 0.0001) ? MeshSize.X : 1.0;
            double DivY = (MeshSize.Y > 0.0001) ? MeshSize.Y : 1.0;

            // Important: Use Mesh.MaxVertexID() to ensure we match the mesh exactly
            for (int32 i = 0; i < Mesh.MaxVertexID(); ++i)
            {
                FVector3d P = Mesh.GetVertex(i);

                // get the standard 0-1 normalized coordinates
                float NormX = (float)((P.X - Min.X) / DivX);
                float NormY = (float)((P.Y - Min.Y) / DivY);

                // Apply 90 degree CCW Rotation:
                // NewU = NormY
                // NewV = 1.0 - NormX
                float RotatedU = 1.0f - NormY;
                float RotatedV = 1.0f - NormX;

                // This fills the array so the next loop doesn't crash!
                GlobalUVHandles.Add(UVOverlay->AppendElement(FVector2f(RotatedU, RotatedV)));
            }

            // --- PASS 3: NORMALS ---
            for (int n = 0; n < 145; n++)
            {
                if (OutChunkNormals.IsValidIndex(chunkindex))
                {
                    FChunkNormals& MyChunk = OutChunkNormals[chunkindex];
                    
                    if (MyChunk.Normals.IsValidIndex(n))
                    {
                        FVector3f VertNormal = MyChunk.Normals[n];

                        float NX = VertNormal.X;
                        float NY = VertNormal.Y;
                        float NZ = VertNormal.Z;

                        FVector3f UENormal(-(float)NZ, (float)NX, (float)NY);
                        NormalHandles.Add(NormalOverlay->AppendElement(UENormal));
                    }
                }
            }

            // --- PASS 4: FACES ---
            uint16 CurrentHoleMask = 0;
            if (OutChunkHeights.IsValidIndex(chunkindex))
            {
                CurrentHoleMask = OutChunkHeights[chunkindex].HoleMask;
            }

            auto GetVertID = [](int32 rowIdx, int32 colIdx, bool bInnerRow) -> int32
                {
                    if (!bInnerRow) return (rowIdx * 17) + colIdx;
                    return (rowIdx * 17) + 9 + colIdx;
                };

            // Loop through the high-res 8x8 quads grid
            for (int32 row = 0; row < 8; ++row)
            {
                for (int32 col = 0; col < 8; ++col)
                {
                    // 1. Scale down from 8x8 quad coordinates to 4x4 macro hole blocks
                    int32 HoleRow = row / 2;
                    int32 HoleCol = col / 2;

                    // 2. Compute the precise bit shift index matching the row/col intersection
                    // Each row takes up exactly 4 bits (Row 0 = 0-3, Row 1 = 4-7, etc.)
                    int32 BitIndex = (HoleRow * 4) + HoleCol;

                    // 3. Test if this specific cell's bit is flagged inside the hexadecimal mask
                    if ((CurrentHoleMask & (1 << BitIndex)) != 0)
                    {
                        // Match confirmed! Skip geometry creation for this quad cell.
                        continue;
                    }

                    // 1. Find the 5 local vertex IDs for this square cell
                    int32 TopLeft = GetVertID(row, col, false);
                    int32 TopRight = GetVertID(row, col + 1, false);
                    int32 BotLeft = GetVertID(row + 1, col, false);
                    int32 BotRight = GetVertID(row + 1, col + 1, false);
                    int32 Center = GetVertID(row, col, true);

                    // 2. Append the 4 triangles with precise CCW winding order
                    int32 T_Top = Mesh.AppendTriangle(TopLeft, Center, TopRight);
                    int32 T_Left = Mesh.AppendTriangle(TopLeft, BotLeft, Center);
                    int32 T_Right = Mesh.AppendTriangle(TopRight, Center, BotRight);
                    int32 T_Bottom = Mesh.AppendTriangle(BotLeft, BotRight, Center);

                    // 3. Bind Attributes
                    auto ApplyFaceAttributes = [&](int32 TriID, int32 ia, int32 ib, int32 ic)
                        {
                            if (TriID != IndexConstants::InvalidID)
                            {
                                if (GlobalUVHandles.IsValidIndex(ia) && GlobalUVHandles.IsValidIndex(ib) && GlobalUVHandles.IsValidIndex(ic))
                                {
                                    UVOverlay->SetTriangle(TriID, UE::Geometry::FIndex3i(GlobalUVHandles[ia], GlobalUVHandles[ib], GlobalUVHandles[ic]));
                                }
                                if (NormalHandles.IsValidIndex(ia) && NormalHandles.IsValidIndex(ib) && NormalHandles.IsValidIndex(ic))
                                {
                                    NormalOverlay->SetTriangle(TriID, UE::Geometry::FIndex3i(NormalHandles[ia], NormalHandles[ib], NormalHandles[ic]));
                                }
                            }
                        };

                    ApplyFaceAttributes(T_Top, TopLeft, Center, TopRight);
                    ApplyFaceAttributes(T_Left, TopLeft, BotLeft, Center);
                    ApplyFaceAttributes(T_Right, TopRight, Center, BotRight);
                    ApplyFaceAttributes(T_Bottom, BotLeft, BotRight, Center);
                }
            }



            FString ActorName = FString::Printf(
                TEXT("%s_%d_%d_%d"),
                *mapname,
                TileX,
                TileY,
                chunkindex
            );



            ADynamicMeshActor* MeshActor = nullptr;

            /* ---------------------------------- */
            /* Look for an existing terrain actor */
            /* ---------------------------------- */

            for (TActorIterator<ADynamicMeshActor> It(GetWorld()); It; ++It)
            {
                if (It->GetActorLabel() == ActorName)
                {
                    MeshActor = *It;
                    break;
                }
            }

            /* ---------------------------------- */
            /* Create new actor if needed         */
            /* ---------------------------------- */

            if (!MeshActor)
            {
                MeshActor = GetWorld()->SpawnActor<ADynamicMeshActor>();

                if (!MeshActor)
                {
                    UE_LOG(LogTemp, Error, TEXT("Bob Builder: Failed to spawn mesh actor."));
                    return;
                }

                MeshActor->SetActorLabel(ActorName);

                FString FinalPath = FString::Printf(TEXT("Terrain/%d_%d"), TileX, TileY);
                MeshActor->SetFolderPath(FName(*FinalPath));

                UE_LOG(LogTemp, Log, TEXT("Bob Builder: created terrain actor %s"), *ActorName);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("Bob Builder: updating terrain actor %s"), *ActorName);
            }

            /* ---------------------------------- */
            /* Update mesh geometry               */
            /* ---------------------------------- */

            UDynamicMeshComponent* MeshComponent = MeshActor->GetDynamicMeshComponent();

            MeshComponent->GetDynamicMesh()->SetMesh(MoveTemp(Mesh));
            MeshComponent->NotifyMeshUpdated();

            UE_LOG(LogTemp, Log, TEXT("Bob Builder: terrain mesh generated for %s"), *ActorName);

            /* ---------------------------------- */
            /* check if we already                */
            /*             have the material made */
            /* ---------------------------------- */
            // 1. Try to find the specific Material Instance for this tile
            FString MIPath = FString::Printf(TEXT("/Game/maps/%s/materials/MI_%s.%s"), *mapname, *ActorName, *ActorName);

            // TRY to load the material
            UMaterialInterface* CurrentMat = LoadObject<UMaterialInstance>(nullptr, *MIPath);

            // IF NOT FOUND, use the Master Debug as a placeholder
            if (!CurrentMat)
            {
                CurrentMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/maps/M_Debug_Terrain"));
                UE_LOG(LogTemp, Warning, TEXT("Bob Builder: Material Instance for mesh %s not found"), *ActorName);
            }

            if (CurrentMat && MeshComponent)
            {
                MeshComponent->SetMaterial(0, CurrentMat);
            }

            MeshComponent->SetNumMaterials(1);

            /* ---------------------------------- */
            /* Enable Collision                   */
            /* ---------------------------------- */

            MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            MeshComponent->SetCollisionProfileName(TEXT("BlockAll"));
            MeshComponent->SetGenerateOverlapEvents(false);

            MeshComponent->UpdateCollision();
        }
    }
}

void ABobBuilder::BuildDoodads(int32 TileX, int32 TileY) {}

void ABobBuilder::BuildWMOs(int32 TileX, int32 TileY) {}

void ABobBuilder::BuildMaterials(int32 TileX, int32 TileY) {}
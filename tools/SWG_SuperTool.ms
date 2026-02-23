-- Star Wars Galaxies Super Tool for 3ds Max (all supported versions)
-- Combines importer/exporter workflows with additional automation helpers.
-- This script expects SWG_MSHImporter.ms and SWG_MSHFullExporter.ms to live in the same directory.

global swgSuper_thisScript
global swgSuper_scriptDir
swgSuper_thisScript = getThisScriptFilename()
swgSuper_scriptDir = if swgSuper_thisScript != undefined then getFilenamePath swgSuper_thisScript else (getDir #scripts + "\\")

global SWG_superStatusLog
if SWG_superStatusLog == undefined do SWG_superStatusLog = #()

global SWG_AllowUnsupportedMaxVersion
global SWG_SupportedMaxYearMin
global SWG_SupportedMaxYearMax
SWG_AllowUnsupportedMaxVersion = true
SWG_SupportedMaxYearMin = 1900
SWG_SupportedMaxYearMax = 9999

fn swgSuper_LogStatus message = (
        append SWG_superStatusLog message
        while SWG_superStatusLog.count > 24 do deleteItem SWG_superStatusLog 1
)

fn swgSuper_IsValidRollout rolloutInstance = (
        rolloutInstance != undefined and (isProperty rolloutInstance #open or isProperty rolloutInstance #controls)
)

fn swgSuper_GetMaxVersionInfo = (
        local major = undefined
        local year = undefined
        local productVersion = undefined

        try(
                local versionData = maxVersion()
                if versionData != undefined and versionData.count > 0 do major = versionData[1]
        ) catch()

        try(productVersion = maxProductVersion()) catch()

        if productVersion != undefined then (
                local tokens = filterString (productVersion as string) "."
                if tokens.count > 0 then (
                        local parsedYear = undefined
                        try(parsedYear = (tokens[1] as integer)) catch(parsedYear = undefined)
                        if parsedYear != undefined and parsedYear > 2000 do year = parsedYear
                )
        )

        if year == undefined and major != undefined do year = major + 1998
        #(year, major, productVersion)
)

fn swgSuper_FormatMaxVersionInfo info = (
        local year = info[1]
        local major = info[2]
        local productVersion = info[3]
        local detail = ""
        if year != undefined do detail += (year as string)
        if major != undefined do detail += (if detail != "" then " (major " else "Major ") + (major as string) + (if detail != "" then ")" else "")
        if productVersion != undefined do detail += (if detail != "" then ", " else "") + "product " + (productVersion as string)
        if detail == "" do detail = "unknown"
        detail
)

fn swgSuper_CheckMaxVersion = (
        local info = swgSuper_GetMaxVersionInfo()
        local detail = swgSuper_FormatMaxVersionInfo info
        local message = "Detected 3ds Max version: " + detail + "."
        if SWG_AllowUnsupportedMaxVersion == true do message += " Override enabled."
        swgSuper_LogStatus message

        if SWG_AllowUnsupportedMaxVersion == true then (
                true
        ) else (
                local year = info[1]
                if year != undefined and year >= SWG_SupportedMaxYearMin and year <= SWG_SupportedMaxYearMax then (
                        true
                ) else (
                        local requiredRange = if SWG_SupportedMaxYearMin == SWG_SupportedMaxYearMax then (SWG_SupportedMaxYearMin as string) else ((SWG_SupportedMaxYearMin as string) + "-" + (SWG_SupportedMaxYearMax as string))
                        messageBox ("This tool supports Autodesk 3ds Max " + requiredRange + ".\nDetected: " + detail + ".\n\nSet global SWG_AllowUnsupportedMaxVersion = true to override.") title:"SWG Super Tool"
                        false
                )
        )
)

global SWG_superVersionSupported
SWG_superVersionSupported = swgSuper_CheckMaxVersion()

-- Prevent the legacy scripts from spawning their standalone floaters while we compose the super tool UI.
try(global SWG_Importer_AutoLaunch = false) catch()
try(global SWG_Exporter_AutoLaunch = false) catch()

fn swgSuper_safeFileIn scriptName = (
        local target = swgSuper_scriptDir + scriptName
        if doesFileExist target then (
                fileIn target
        ) else (
                messageBox ("Unable to locate required script: " + target) title:"SWG Super Tool"
        )
)

swgSuper_safeFileIn "SWG_MSHImporter.ms"
swgSuper_safeFileIn "SWG_MSHFullExporter.ms"

-- Global state for the super tool.
global SWG_SuperToolFloater
global SWG_superCurrentRollout
global SWG_superPanelControl

global SWG_superOutputRoot
if SWG_superOutputRoot == undefined do SWG_superOutputRoot = ""

global SWG_superAssetName
if SWG_superAssetName == undefined do SWG_superAssetName = ""

global SWG_superAutoFixMeshQuality
if SWG_superAutoFixMeshQuality == undefined do SWG_superAutoFixMeshQuality = true

global SWG_superAutoFixWeldThreshold
if SWG_superAutoFixWeldThreshold == undefined do SWG_superAutoFixWeldThreshold = 0.001

global SWG_superAutoFixSmoothAngle
if SWG_superAutoFixSmoothAngle == undefined do SWG_superAutoFixSmoothAngle = 45.0

global SWG_superSkeletonRoot

global SWG_superAnimationLibrary
if SWG_superAnimationLibrary == undefined do SWG_superAnimationLibrary = #()

global SWG_superDashboard
global SWG_superUtilities
global SWG_superGuidedRigging
global SWG_superGuidedFloater
global SWG_superGuidedStep
global SWG_superGuidedRigType

if SWG_superGuidedStep == undefined do SWG_superGuidedStep = 1
if SWG_superGuidedRigType == undefined do SWG_superGuidedRigType = "Auto"

global SWG_superSkeletonValidationConfig
if SWG_superSkeletonValidationConfig == undefined do SWG_superSkeletonValidationConfig = #(
        #("auto", 12, 3),
        #("biped", 20, 4),
        #("quadruped", 24, 4)
)

fn swgSuper_SanitizeAssetName rawName = (
        local name = if rawName == undefined then "" else toLower rawName
        name = substituteString name " " "_"
        name = substituteString name "-" "_"
        name = substituteString name "." "_"
        local allowed = "abcdefghijklmnopqrstuvwxyz0123456789_"
        local result = ""
        for i = 1 to name.count do (
                local ch = name[i]
                if findString allowed ch != undefined then result += ch
        )
        while (result.count > 0 and result[1] == "_") do (
                result = if result.count > 1 then substring result 2 (result.count - 1) else ""
        )
        while findString result "__" != undefined do (
                result = substituteString result "__" "_"
        )
        if result == "" then result = "swg_asset"
        result
)

fn swgSuper_SetOutputRoot path = (
        if path != undefined and path != "" then (
                SWG_superOutputRoot = path
                if SWG_superDashboard != undefined do (
                        SWG_superDashboard.lblOutput.text = "Output: " + path
                )
                swgSuper_UpdateUtilities()
        )
)

fn swgSuper_SetAssetName rawName = (
        local sanitized = swgSuper_SanitizeAssetName rawName
        SWG_superAssetName = sanitized
        if SWG_superDashboard != undefined do (
                if SWG_superDashboard.edtAssetName.text != sanitized then (
                        SWG_superDashboard.edtAssetName.text = sanitized
                )
        )
        swgSuper_UpdateUtilities()
        sanitized
)

fn swgSuper_IsBoneNode node = (
        local isBoneNode = false
        if node != undefined and isValidNode node then (
                try(isBoneNode = isBone node) catch(isBoneNode = false)
                if not isBoneNode then (
                        if isProperty node #isBone then (
                                try(isBoneNode = node.isBone) catch(isBoneNode = false)
                        )
                )
                if not isBoneNode then (
                        local lowerName = toLower node.name
                        if matchPattern lowerName pattern:"*bone*" then isBoneNode = true
                )
        )
        isBoneNode
)

fn swgSuper_FindSkeletonRoot nodes = (
        local fallback = undefined
        if nodes != undefined then (
                for node in nodes do (
                        if node != undefined and isValidNode node then (
                                if swgSuper_IsBoneNode node then (
                                        local lowerName = toLower node.name
                                        if matchPattern lowerName pattern:"*root*" then return node
                                        if matchPattern lowerName pattern:"*pelvis*" then fallback = node
                                        if fallback == undefined then fallback = node
                                )
                        )
                )
        )
        fallback
)

fn swgSuper_SetSkeletonRoot node silent:false = (
        if node != undefined and isValidNode node then (
                SWG_superSkeletonRoot = node
                if not silent then swgSuper_Log ("Skeleton root set to " + node.name)
                swgSuper_UpdateUtilities()
        )
)

fn swgSuper_GatherSkeletonNodes rootNode = (
        local result = #()
        if rootNode == undefined or not isValidNode rootNode then return result
        local stack = #(rootNode)
        while stack.count > 0 do (
                local n = stack[stack.count]
                deleteItem stack stack.count
                if n != undefined and isValidNode n then (
                        if swgSuper_IsBoneNode n then (
                                if (findItem result n) == 0 then append result n
                        )
                        for c in n.children do (
                                append stack c
                        )
                )
        )
        result
)

fn swgSuper_GetSkeletonValidationConfig assetType = (
        local resolved = if assetType == undefined then "auto" else toLower assetType
        local minBones = 12
        local minDepth = 3
        for entry in SWG_superSkeletonValidationConfig do (
                if entry.count >= 3 and (toLower entry[1]) == resolved then (
                        minBones = entry[2]
                        minDepth = entry[3]
                        exit
                )
        )
        #(resolved, minBones, minDepth)
)

fn swgSuper_ComputeSkeletonDepth rootNode = (
        if rootNode == undefined or not isValidNode rootNode then return 0
        local maxDepth = 0
        local stack = #(#(rootNode, 1))
        while stack.count > 0 do (
                local entry = stack[stack.count]
                deleteItem stack stack.count
                local node = entry[1]
                local depth = entry[2]
                if node != undefined and isValidNode node and swgSuper_IsBoneNode node then (
                        if depth > maxDepth then maxDepth = depth
                        for c in node.children do (
                                if swgSuper_IsBoneNode c then append stack #(c, depth + 1)
                        )
                )
        )
        maxDepth
)

fn swgSuper_ValidateSkeleton assetType:SWG_superGuidedRigType promptOnFailure:true = (
        local config = swgSuper_GetSkeletonValidationConfig assetType
        local resolvedType = config[1]
        local minBones = config[2]
        local minDepth = config[3]
        local warnings = #()

        if SWG_superSkeletonRoot == undefined or not isValidNode SWG_superSkeletonRoot then (
                append warnings "No skeleton root is set."
        ) else (
                if not swgSuper_IsBoneNode SWG_superSkeletonRoot then (
                        append warnings ("Skeleton root '" + SWG_superSkeletonRoot.name + "' is not a bone.")
                )
        )

        local skeletonNodes = #()
        if SWG_superSkeletonRoot != undefined and isValidNode SWG_superSkeletonRoot then (
                skeletonNodes = swgSuper_GatherSkeletonNodes SWG_superSkeletonRoot
        )

        if skeletonNodes.count < minBones then (
                append warnings ("Skeleton has " + (skeletonNodes.count as string) + " bones; expected at least " + (minBones as string) + " for " + resolvedType + ".")
        )

        local depth = swgSuper_ComputeSkeletonDepth SWG_superSkeletonRoot
        if depth < minDepth then (
                append warnings ("Skeleton depth is " + (depth as string) + "; expected at least " + (minDepth as string) + " levels for " + resolvedType + ".")
        )

        if warnings.count > 0 then (
                swgSuper_Log "Warning: Skeleton validation failed."
                for warning in warnings do swgSuper_Log ("Warning: " + warning)
                if promptOnFailure then (
                        local prompt = "Skeleton validation failed for " + resolvedType + ":\n\n"
                        for warning in warnings do prompt += ("- " + warning + "\n")
                        prompt += "\nProceed anyway?"
                        if queryBox prompt title:"SWG Super Tool" then (
                                swgSuper_Log "Proceeding despite skeleton validation warnings."
                                true
                        ) else (
                                swgSuper_Log "Skeleton validation blocked the operation."
                                false
                        )
                ) else (
                        false
                )
        ) else (
                swgSuper_Log ("Skeleton validation passed for " + resolvedType + ".")
                true
        )
)

fn swgSuper_AddOrUpdateClip clipName startFrame endFrame = (
        if clipName == undefined or clipName == "" then return false
        local existingIndex = 0
        for i = 1 to SWG_superAnimationLibrary.count do (
                if toLower SWG_superAnimationLibrary[i][1] == toLower clipName then (
                        existingIndex = i
                        exit
                )
        )
        local record = #(clipName, startFrame as integer, endFrame as integer)
        if existingIndex > 0 then (
                SWG_superAnimationLibrary[existingIndex] = record
        ) else (
                append SWG_superAnimationLibrary record
        )
        swgSuper_UpdateAnimationList()
        true
)

fn swgSuper_AutoConfigureFromScene = (
        local nodes = selection as array
        local candidateName = undefined
        if nodes.count > 0 then candidateName = nodes[1].name
        if candidateName == undefined or candidateName == "" then (
                try(candidateName = getFileNameFile maxFileName) catch(candidateName = undefined)
        )
        if candidateName == undefined or candidateName == "" then candidateName = "swg_asset"
        local sanitized = swgSuper_SetAssetName candidateName
        swgSuper_Log ("Asset tag set to " + sanitized + " via smart configure.")

        if SWG_superOutputRoot == undefined or SWG_superOutputRoot == "" then (
                local defaultPath = undefined
                try(defaultPath = maxFilePath) catch(defaultPath = undefined)
                if defaultPath == undefined or defaultPath == "" then (
                        try(defaultPath = getDir #export) catch(defaultPath = undefined)
                )
                if defaultPath != undefined and defaultPath != "" then (
                        swgSuper_SetOutputRoot defaultPath
                        swgSuper_Log ("Output folder auto-set to " + defaultPath)
                )
        )

        local root = swgSuper_FindSkeletonRoot nodes
        if root == undefined then (
                root = swgSuper_FindSkeletonRoot objects
        )
        if root != undefined then (
                swgSuper_SetSkeletonRoot root
        ) else (
                swgSuper_Log "No skeleton root detected. Select a bone and try again."
        )

        local startFrame = animationRange.start.frame as integer
        local endFrame = animationRange.end.frame as integer
        if swgSuper_AddOrUpdateClip "full_body" startFrame endFrame then (
                swgSuper_Log "Default 'full_body' clip registered."
        )
        swgSuper_Log "Scene smart configuration complete."
)

fn swgSuper_RunExportWizard bakeAnimations = (
        local nodes = selection as array
        if nodes.count == 0 then (
                messageBox "Select the geometry you want to export before running Smart Export." title:"SWG Super Tool"
                return undefined
        )
        if not (swgSuper_ValidateSkeleton assetType:SWG_superGuidedRigType promptOnFailure:true) then return undefined
        swgSuper_Log ("Smart export running on " + (nodes.count as string) + " nodes.")
        if not swgSuper_PrepareGeometry nodes then return undefined

        local skeletonNodes = #()
        if SWG_superSkeletonRoot != undefined and isValidNode SWG_superSkeletonRoot then (
                skeletonNodes = swgSuper_GatherSkeletonNodes SWG_superSkeletonRoot
                if skeletonNodes.count > 0 then (
                        local skeletonReady = swgSuper_PrepareSkeleton skeletonNodes
                        if skeletonReady and bakeAnimations and SWG_superAnimationLibrary.count > 0 then (
                                for clip in SWG_superAnimationLibrary do (
                                        swgSuper_BakeAnimation skeletonNodes clip[2] clip[3]
                                )
                        )
                )
        ) else (
                swgSuper_Log "Smart export skipped skeleton prep because no root is set."
        )

        swgSuper_Log "Triggering exporter dialog."
        swgSuper_RunExporter()
)

fn swgSuper_GetOutputPath extension = (
        if (SWG_superOutputRoot == undefined or SWG_superOutputRoot == "" or SWG_superAssetName == undefined or SWG_superAssetName == "") then (
                "<asset or output path not set>"
        ) else (
                SWG_superOutputRoot + "\\" + SWG_superAssetName + "." + extension
        )
)

fn swgSuper_UpdateAnimationList = (
        if SWG_superUtilities != undefined do (
                local items = for clip in SWG_superAnimationLibrary collect (clip[1] + " [" + (clip[2] as string) + "-" + (clip[3] as string) + "]")
                SWG_superUtilities.lbAnimations.items = items
        )
)

fn swgSuper_UpdateUtilities = (
        if SWG_superUtilities != undefined do (
                SWG_superUtilities.lblMGNPath.text = "MGN: " + swgSuper_GetOutputPath "mgn"
                SWG_superUtilities.lblMSHPath.text = "MSH: " + swgSuper_GetOutputPath "msh"
                SWG_superUtilities.lblSKTPath.text = "SKT: " + swgSuper_GetOutputPath "skt"
                if (SWG_superSkeletonRoot != undefined and isValidNode SWG_superSkeletonRoot) then (
                        SWG_superUtilities.lblSkeletonRoot.text = "Skeleton Root: " + SWG_superSkeletonRoot.name
                ) else (
                        SWG_superUtilities.lblSkeletonRoot.text = "Skeleton Root: <not set>"
                )
                swgSuper_UpdateAnimationList()
        )
        if SWG_superDashboard != undefined do (
                if SWG_superOutputRoot != undefined and SWG_superOutputRoot != "" then (
                        SWG_superDashboard.lblOutput.text = "Output: " + SWG_superOutputRoot
                ) else (
                        SWG_superDashboard.lblOutput.text = "Output: <not set>"
                )
        )
)

fn swgSuper_Log message = (
        swgSuper_LogStatus message
        if SWG_superDashboard != undefined do (
                local str = ""
                for m in SWG_superStatusLog do str += (m + "\n")
                if isProperty SWG_superDashboard #logOutputDotNet and SWG_superDashboard.logOutputDotNet.visible == true then (
                        try(SWG_superDashboard.logOutputDotNet.text = str) catch()
                )
                if isProperty SWG_superDashboard #logOutputFallback and SWG_superDashboard.logOutputFallback.visible == true then (
                        SWG_superDashboard.logOutputFallback.text = str
                )
                SWG_superDashboard.lblStatus.text = "Status: " + message
        )
)

fn swgSuper_ShowRollout ctrl roll = (
        if ctrl == undefined then return undefined
        if roll == undefined then return undefined
        if SWG_superCurrentRollout != undefined do (
                try(removeSubRollout ctrl SWG_superCurrentRollout) catch()
        )
        addSubRollout ctrl roll
        SWG_superCurrentRollout = roll
        roll
)

fn swgSuper_NormalizeNodes nodes = (
        if nodes == undefined then (
                #()
        ) else if isKindOf nodes Array then (
                nodes
        ) else (
                #(nodes)
        )
)

fn swgSuper_ComputeAutoSmoothAngle obj = (
        local angles = #()
        local isPoly = isKindOf obj Editable_Poly
        try(
                local edgeCount = if isPoly then polyop.getNumEdges obj else meshop.getNumEdges obj
                for e = 1 to edgeCount do (
                        local faces = if isPoly then polyop.getEdgeFaces obj e else meshop.getEdgeFaces obj e
                        if faces != undefined and faces.count == 2 then (
                                local f1 = faces[1]
                                local f2 = faces[2]
                                if f1 > 0 and f2 > 0 do (
                                        local n1 = if isPoly then polyop.getFaceNormal obj f1 else meshop.getFaceNormal obj f1
                                        local n2 = if isPoly then polyop.getFaceNormal obj f2 else meshop.getFaceNormal obj f2
                                        local dotVal = dot n1 n2
                                        dotVal = amax -1.0 (amin 1.0 dotVal)
                                        local angle = (acos dotVal) * 180.0 / pi
                                        append angles angle
                                )
                        )
                )
        ) catch()
        if angles.count == 0 then (
                45.0
        ) else (
                qsort angles
                local pct = 0.8
                local idx = amax 1 (amin angles.count (ceil (angles.count * pct)))
                angles[idx]
        )
)

fn swgSuper_AutoFixMeshQuality obj weldThreshold smoothAngle = (
        local welded = false
        local normals = false
        local smoothed = false
        local degenerateRemoved = 0
        local isoRemoved = 0
        local computedAngle = smoothAngle

        if obj == undefined or not isValidNode obj then return #(welded, normals, smoothed, degenerateRemoved, isoRemoved)

        if weldThreshold > 0 do (
                try(
                        if isKindOf obj Editable_Poly then (
                                local vertCount = polyop.getNumVerts obj
                                if vertCount > 0 do polyop.weldVertsByThreshold obj #{1..vertCount} weldThreshold
                        ) else if isKindOf obj Editable_Mesh then (
                                meshop.weldVertsByThreshold obj weldThreshold
                        )
                        welded = true
                ) catch()
        )

        try(
                local hasExplicit = false
                local missingNormals = false
                local meshSnapshot = undefined
                try(meshSnapshot = snapshotAsMesh obj) catch(meshSnapshot = undefined)
                if meshSnapshot != undefined then (
                        local normalCount = 0
                        try(normalCount = meshSnapshot.numNormals) catch(normalCount = 0)
                        if normalCount == 0 then (
                                missingNormals = true
                        ) else (
                                for n = 1 to normalCount do (
                                        local isExplicit = false
                                        try(isExplicit = meshop.getNormalExplicit meshSnapshot n) catch(isExplicit = false)
                                        if isExplicit do (
                                                hasExplicit = true
                                                exit
                                        )
                                )
                        )
                        free meshSnapshot
                )
                if (missingNormals or (not hasExplicit)) do (
                        local normalMod = Edit_Normals()
                        addModifier obj normalMod
                        try(normalMod.Reset()) catch()
                        try(normalMod.MakeExplicit()) catch()
                        try(deleteModifier obj normalMod) catch(try(collapseStack obj) catch())
                        normals = true
                )
        ) catch()

        if smoothAngle < 0 do (
                computedAngle = swgSuper_ComputeAutoSmoothAngle obj
                computedAngle = amax 0.0 (amin 180.0 computedAngle)
                swgSuper_Log ("Auto smoothing angle computed for " + obj.name + ": " + (computedAngle as string) + " degrees.")
        )

        if computedAngle >= 0 do (
                try(
                        if isKindOf obj Editable_Poly then (
                                polyop.autoSmooth obj computedAngle
                        ) else if isKindOf obj Editable_Mesh then (
                                meshop.autoSmooth obj computedAngle
                        )
                        smoothed = true
                ) catch()
        )

        try(
                local faceCount = 0
                if isKindOf obj Editable_Poly then faceCount = polyop.getNumFaces obj else if isKindOf obj Editable_Mesh then faceCount = meshop.getNumFaces obj
                if faceCount > 0 then (
                        local degenerateFaces = #{}
                        for f = 1 to faceCount do (
                                local area = 0.0
                                try(area = polyop.getFaceArea obj f) catch(try(area = meshop.getFaceArea obj f) catch(area = 0.0))
                                if area <= 0.000001 do degenerateFaces[f] = true
                        )
                        if degenerateFaces.numberset > 0 then (
                                try(polyop.deleteFaces obj degenerateFaces) catch(try(meshop.deleteFaces obj degenerateFaces) catch())
                                degenerateRemoved = degenerateFaces.numberset
                        )
                )
                local vertCountBefore = 0
                local vertCountAfter = 0
                try(
                        vertCountBefore = if isKindOf obj Editable_Poly then polyop.getNumVerts obj else if isKindOf obj Editable_Mesh then meshop.getNumVerts obj else 0
                        try(polyop.deleteIsoVerts obj) catch(try(meshop.deleteIsoVerts obj) catch())
                        vertCountAfter = if isKindOf obj Editable_Poly then polyop.getNumVerts obj else if isKindOf obj Editable_Mesh then meshop.getNumVerts obj else vertCountBefore
                ) catch()
                if vertCountBefore > vertCountAfter do isoRemoved = vertCountBefore - vertCountAfter
        ) catch()

        #(welded, normals, smoothed, degenerateRemoved, isoRemoved)
)

struct SWGSuperMeshQualityCounts
(
        degenerateFaces = 0,
        unweldedVertices = 0,
        missingSmoothingFaces = 0,
        mixedNormalsFaces = 0,
        nonManifoldEdges = 0
)

fn swgSuper_ValidateMeshQualityLite nodes = (
        local counts = SWGSuperMeshQualityCounts()
        local areaEpsilon = 0.0001
        local weldKeyScale = 1000.0

        for obj in nodes do (
                if isKindOf obj geometryclass then (
                        local mesh = undefined
                        try(mesh = snapshotAsMesh obj) catch(mesh = undefined)
                        if mesh != undefined then (
                                local degenerateFaces = 0
                                local missingSmoothing = 0
                                local mixedNormals = 0
                                local unweldedVertices = 0
                                local nonManifoldEdges = 0

                                for i = 1 to mesh.numfaces do (
                                        local face = getFace mesh i
                                        local v1 = getVert mesh face.x
                                        local v2 = getVert mesh face.y
                                        local v3 = getVert mesh face.z
                                        local area = (length (cross (v2 - v1) (v3 - v1))) * 0.5
                                        if area < areaEpsilon do degenerateFaces += 1

                                        local smoothGroup = 0
                                        try(smoothGroup = getFaceSmoothGroup mesh i) catch(smoothGroup = 0)
                                        if smoothGroup == 0 do missingSmoothing += 1
                                )

                                local normalSum = [0,0,0]
                                for i = 1 to mesh.numfaces do (
                                        local faceNormal = [0,0,0]
                                        try(faceNormal = meshop.getFaceNormal mesh i) catch(faceNormal = [0,0,0])
                                        normalSum += faceNormal
                                )
                                if (length normalSum) > 0.0001 then (
                                        local avgNormal = normalize normalSum
                                        for i = 1 to mesh.numfaces do (
                                                local faceNormal = [0,0,0]
                                                try(faceNormal = meshop.getFaceNormal mesh i) catch(faceNormal = [0,0,0])
                                                if (dot (normalize faceNormal) avgNormal) < -0.25 do mixedNormals += 1
                                        )
                                )

                                local vertsHash = dotNetObject "System.Collections.Hashtable"
                                for i = 1 to mesh.numverts do (
                                        local v = getVert mesh i
                                        local key = ((round (v.x * weldKeyScale)) as string) + "_" + ((round (v.y * weldKeyScale)) as string) + "_" + ((round (v.z * weldKeyScale)) as string)
                                        if vertsHash.ContainsKey key then (
                                                vertsHash.Item[key] = (vertsHash.Item[key]) + 1
                                        ) else (
                                                vertsHash.Add key 1
                                        )
                                )
                                for entry in vertsHash.Values do if entry > 1 do unweldedVertices += (entry - 1)

                                try(
                                        local edgeCount = meshop.getNumEdges mesh
                                        for e = 1 to edgeCount do (
                                                local faces = meshop.getFacesUsingEdge mesh e
                                                if faces.count > 2 do nonManifoldEdges += 1
                                        )
                                ) catch()

                                counts.degenerateFaces += degenerateFaces
                                counts.unweldedVertices += unweldedVertices
                                counts.missingSmoothingFaces += missingSmoothing
                                counts.mixedNormalsFaces += mixedNormals
                                counts.nonManifoldEdges += nonManifoldEdges
                        )
                )
        )

        counts
)

fn swgSuper_FormatMeshQualitySummary counts = (
        local totalIssues = counts.degenerateFaces + counts.unweldedVertices + counts.missingSmoothingFaces + counts.mixedNormalsFaces + counts.nonManifoldEdges
        local summary = "Mesh quality validation: " + (totalIssues as string) + " issue(s) remaining. "
        summary += "Degenerate " + (counts.degenerateFaces as string)
        summary += ", unwelded " + (counts.unweldedVertices as string)
        summary += ", smoothing " + (counts.missingSmoothingFaces as string)
        summary += ", mixed normals " + (counts.mixedNormalsFaces as string)
        summary += ", non-manifold " + (counts.nonManifoldEdges as string) + "."
        summary
)

fn swgSuper_PrepareGeometry nodes = (
        nodes = swgSuper_NormalizeNodes nodes
        if nodes.count == 0 then (
                messageBox "Select one or more mesh nodes to prepare." title:"SWG Super Tool"
                false
        ) else (
                local processed = 0
                local weldedNodes = 0
                local normalNodes = 0
                local smoothedNodes = 0
                local degenerateFacesRemoved = 0
                local isoVertsRemoved = 0
                local autoFixEnabled = (SWG_superAutoFixMeshQuality == true)
                local weldThreshold = SWG_superAutoFixWeldThreshold
                local smoothAngle = SWG_superAutoFixSmoothAngle

                if autoFixEnabled do (
                        local smoothLabel = if smoothAngle < 0 then "Auto" else (smoothAngle as string)
                        swgSuper_Log ("Auto Fix Mesh Quality enabled (weld threshold: " + (weldThreshold as string) + ", smoothing angle: " + smoothLabel + ").")
                )

                for obj in nodes do (
                        if obj != undefined and isValidNode obj then (
                                try(resetXForm obj) catch()
                                if (canConvertTo obj Editable_Poly) do (
                                        try(convertToEditablePoly obj) catch()
                                )
                                try(collapseStack obj) catch()
                                if autoFixEnabled do (
                                        local cleanupResult = swgSuper_AutoFixMeshQuality obj weldThreshold smoothAngle
                                        if cleanupResult[1] do weldedNodes += 1
                                        if cleanupResult[2] do normalNodes += 1
                                        if cleanupResult[3] do smoothedNodes += 1
                                        degenerateFacesRemoved += cleanupResult[4]
                                        isoVertsRemoved += cleanupResult[5]
                                )
                                processed += 1
                        )
                )
                if processed > 0 then (
                        swgSuper_Log ("Prepared geometry for " + (processed as string) + " nodes.")
                        if autoFixEnabled do (
                                swgSuper_Log ("Mesh cleanup applied: welded " + (weldedNodes as string) + " nodes, recomputed normals on " + (normalNodes as string) + " nodes, auto-smoothed " + (smoothedNodes as string) + " nodes, removed " + (degenerateFacesRemoved as string) + " degenerate faces, removed " + (isoVertsRemoved as string) + " isolated verts.")
                                local qualityCounts = swgSuper_ValidateMeshQualityLite nodes
                                swgSuper_Log (swgSuper_FormatMeshQualitySummary qualityCounts)
                        )
                        true
                ) else (
                        swgSuper_Log "No valid geometry nodes were prepared."
                        false
                )
        )
)

fn swgSuper_PrepareSkeleton nodes = (
        nodes = swgSuper_NormalizeNodes nodes
        if nodes.count == 0 then (
                messageBox "Select the skeleton bones you want to prep." title:"SWG Super Tool"
                false
        ) else (
                local processed = 0
                for b in nodes do (
                        if b != undefined and isValidNode b then (
                                try(if isProperty b #boneScaleType do b.boneScaleType = #none) catch()
                                try(if isProperty b #showLinks do b.showLinks = true) catch()
                                try(if isProperty b #freeze do b.freeze = false) catch()
                                try(if isProperty b #boneObject do b.boneObject = true) catch()
                                processed += 1
                        )
                )
                if processed > 0 then (
                        swgSuper_Log ("Prepared skeleton nodes: " + (processed as string))
                        true
                ) else (
                        swgSuper_Log "No valid skeleton nodes were prepared."
                        false
                )
        )
)

fn swgSuper_ComputeBounds nodes = (
        local minPt = [1e9, 1e9, 1e9]
        local maxPt = [-1e9, -1e9, -1e9]
        for n in nodes do (
                if n != undefined and isValidNode n then (
                        local bb = nodeGetBoundingBox n n.transform
                        local bbMin = bb[1]
                        local bbMax = bb[2]
                        minPt = [amin minPt.x bbMin.x, amin minPt.y bbMin.y, amin minPt.z bbMin.z]
                        maxPt = [amax maxPt.x bbMax.x, amax maxPt.y bbMax.y, amax maxPt.z bbMax.z]
                )
        )
        #(minPt, maxPt)
)

fn swgSuper_CreateBone name startPos endPos = (
        local bone = boneSys.createBone startPos endPos [0, 0, 1]
        if bone != undefined do bone.name = name
        bone
)

fn swgSuper_CollectVertexSamples nodes maxSamples:5000 = (
        local samples = #()
        if maxSamples < 1 then maxSamples = 1
        for n in nodes do (
                if n != undefined and isValidNode n then (
                        local meshSnapshot = snapshotAsMesh n
                        if meshSnapshot != undefined then (
                                local vertCount = meshSnapshot.numverts
                                if vertCount > 0 then (
                                        local step = amax 1 (ceiling (vertCount as float / maxSamples))
                                        for i = 1 to vertCount by step do (
                                                local v = getVert meshSnapshot i
                                                local worldV = v * n.transform
                                                append samples worldV
                                        )
                                )
                                free meshSnapshot
                        )
                )
        )
        samples
)

fn swgSuper_Percentile values percent = (
        if values.count == 0 then undefined else (
                local sorted = copy values
                sort sorted
                local idx = amax 1 (amin sorted.count (ceiling (sorted.count as float * percent)))
                sorted[idx]
        )
)

fn swgSuper_ComputeSpanAtHeight vertices targetZ band = (
        local minX = 1e9
        local maxX = -1e9
        local minY = 1e9
        local maxY = -1e9
        local count = 0
        for v in vertices do (
                if (abs (v.z - targetZ)) <= band then (
                        minX = amin minX v.x
                        maxX = amax maxX v.x
                        minY = amin minY v.y
                        maxY = amax maxY v.y
                        count += 1
                )
        )
        if count < 5 then (
                undefined
        ) else (
                #([minX, maxX], [minY, maxY], count)
        )
)

fn swgSuper_AverageAxisAtHeight vertices minZ maxZ axis:#y = (
        local sum = 0.0
        local count = 0
        for v in vertices do (
                if v.z >= minZ and v.z <= maxZ then (
                        sum += (if axis == #x then v.x else v.y)
                        count += 1
                )
        )
        if count == 0 then undefined else (sum / count)
)

fn swgSuper_AutoRigSkeleton rigType:"auto" = (
        local nodes = for n in selection where (n != undefined and isValidNode n and (superClassOf n == GeometryClass)) collect n
        if nodes.count == 0 then (
                messageBox "Select one or more mesh nodes to auto-rig." title:"SWG Super Tool"
                return false
        )

        local bounds = swgSuper_ComputeBounds nodes
        local minPt = bounds[1]
        local maxPt = bounds[2]
        local center = (minPt + maxPt) * 0.5
        local size = maxPt - minPt
        local height = amax 1.0 size.z
        local width = amax 1.0 size.x
        local depth = amax 1.0 size.y

        local symmetryAxis = if width >= depth then "X" else "Y"
        swgSuper_Log ("Auto-rig symmetry plane: " + symmetryAxis + " = " + (if symmetryAxis == "X" then center.x as string else center.y as string))

        local resolvedRigType = toLower rigType
        local isAnimal = depth > (width * 1.3)
        if resolvedRigType == "biped" then (
                isAnimal = false
        ) else if resolvedRigType == "quadruped" then (
                isAnimal = true
        )
        local bones = #()

        local vertices = swgSuper_CollectVertexSamples nodes maxSamples:6000
        local pelvisZ = swgSuper_Percentile (for v in vertices collect v.z) 0.45
        local spine1Z = swgSuper_Percentile (for v in vertices collect v.z) 0.58
        local spine2Z = swgSuper_Percentile (for v in vertices collect v.z) 0.70
        local chestZ = swgSuper_Percentile (for v in vertices collect v.z) 0.82
        local neckZ = swgSuper_Percentile (for v in vertices collect v.z) 0.90
        local headZ = swgSuper_Percentile (for v in vertices collect v.z) 0.96

        local rootPos = [center.x, center.y, minPt.z]
        local pelvisPos = [center.x, center.y, if pelvisZ != undefined then pelvisZ else (minPt.z + (height * 0.45))]
        local spine1Pos = [center.x, center.y, if spine1Z != undefined then spine1Z else (minPt.z + (height * 0.58))]
        local spine2Pos = [center.x, center.y, if spine2Z != undefined then spine2Z else (minPt.z + (height * 0.70))]
        local chestPos = [center.x, center.y, if chestZ != undefined then chestZ else (minPt.z + (height * 0.82))]
        local neckPos = [center.x, center.y, if neckZ != undefined then neckZ else (minPt.z + (height * 0.90))]
        local headPos = [center.x, center.y, if headZ != undefined then headZ else (minPt.z + (height * 0.96))]
        local headTip = [center.x, center.y, if headZ != undefined then (headZ + (height * 0.09)) else (minPt.z + (height * 1.05))]

        local rootBone = swgSuper_CreateBone "root" rootPos pelvisPos
        local pelvisBone = swgSuper_CreateBone "pelvis" pelvisPos spine1Pos
        local spine1Bone = swgSuper_CreateBone "spine_01" spine1Pos spine2Pos
        local spine2Bone = swgSuper_CreateBone "spine_02" spine2Pos chestPos
        local chestBone = swgSuper_CreateBone "chest" chestPos neckPos
        local neckBone = swgSuper_CreateBone "neck" neckPos headPos
        local headBone = swgSuper_CreateBone "head" headPos headTip

        pelvisBone.parent = rootBone
        spine1Bone.parent = pelvisBone
        spine2Bone.parent = spine1Bone
        chestBone.parent = spine2Bone
        neckBone.parent = chestBone
        headBone.parent = neckBone

        append bones rootBone
        append bones pelvisBone
        append bones spine1Bone
        append bones spine2Bone
        append bones chestBone
        append bones neckBone
        append bones headBone

        local hipOffset = width * 0.18
        local shoulderOffset = width * 0.22
        local pelvisSpan = swgSuper_ComputeSpanAtHeight vertices pelvisPos.z (height * 0.04)
        if pelvisSpan != undefined then (
                hipOffset = ((pelvisSpan[1].y - pelvisSpan[1].x) * 0.35)
        )
        local chestSpan = swgSuper_ComputeSpanAtHeight vertices chestPos.z (height * 0.05)
        if chestSpan != undefined then (
                shoulderOffset = ((chestSpan[1].y - chestSpan[1].x) * 0.45)
        )
        local thighLen = height * 0.22
        local calfLen = height * 0.22
        local footLen = height * 0.08
        local upperArmLen = height * 0.18
        local foreArmLen = height * 0.18
        local handLen = height * 0.08

        local legY = if isAnimal then (center.y - (depth * 0.12)) else center.y
        local frontLegY = if isAnimal then (center.y + (depth * 0.18)) else center.y
        local lowerAvgY = swgSuper_AverageAxisAtHeight vertices minPt.z pelvisPos.z axis:#y
        if lowerAvgY != undefined then (
                legY = (center.y + lowerAvgY) * 0.5
        )
        local upperAvgY = swgSuper_AverageAxisAtHeight vertices chestPos.z maxPt.z axis:#y
        if isAnimal and upperAvgY != undefined then (
                frontLegY = (center.y + upperAvgY) * 0.5
        )

        swgSuper_Log ("Auto-rig offsets: hip=" + (hipOffset as string) + " shoulder=" + (shoulderOffset as string) + " legY=" + (legY as string) + " frontLegY=" + (frontLegY as string))

        fn buildLeg sideName basePos parentBone thighLength calfLength footLength rigDepth bonesArr = (
                local kneePos = basePos + [0, 0, -thighLength]
                local anklePos = kneePos + [0, 0, -calfLength]
                local toePos = anklePos + [0, (rigDepth * 0.15), -(footLength * 0.2)]
                local thigh = swgSuper_CreateBone (sideName + "_thigh") basePos kneePos
                local calf = swgSuper_CreateBone (sideName + "_calf") kneePos anklePos
                local foot = swgSuper_CreateBone (sideName + "_foot") anklePos toePos
                thigh.parent = parentBone
                calf.parent = thigh
                foot.parent = calf
                append bonesArr thigh
                append bonesArr calf
                append bonesArr foot
        )

        fn buildArm sideName basePos parentBone upperArmLength foreArmLength handLength rigDepth bonesArr = (
                local elbowPos = basePos + [0, (rigDepth * 0.12), -upperArmLength]
                local wristPos = elbowPos + [0, (rigDepth * 0.08), -foreArmLength]
                local handPos = wristPos + [0, (rigDepth * 0.06), -(handLength * 0.2)]
                local upper = swgSuper_CreateBone (sideName + "_upperarm") basePos elbowPos
                local lower = swgSuper_CreateBone (sideName + "_forearm") elbowPos wristPos
                local hand = swgSuper_CreateBone (sideName + "_hand") wristPos handPos
                upper.parent = parentBone
                lower.parent = upper
                hand.parent = lower
                append bonesArr upper
                append bonesArr lower
                append bonesArr hand
        )

        buildLeg "l" [center.x + hipOffset, legY, pelvisPos.z] pelvisBone thighLen calfLen footLen depth bones
        buildLeg "r" [center.x - hipOffset, legY, pelvisPos.z] pelvisBone thighLen calfLen footLen depth bones

        if isAnimal then (
                buildLeg "l_front" [center.x + hipOffset, frontLegY, chestPos.z] chestBone thighLen calfLen footLen depth bones
                buildLeg "r_front" [center.x - hipOffset, frontLegY, chestPos.z] chestBone thighLen calfLen footLen depth bones
        ) else (
                buildArm "l" [center.x + shoulderOffset, center.y, chestPos.z] chestBone upperArmLen foreArmLen handLen depth bones
                buildArm "r" [center.x - shoulderOffset, center.y, chestPos.z] chestBone upperArmLen foreArmLen handLen depth bones
        )

        SWG_superSkeletonRoot = rootBone
        swgSuper_PrepareSkeleton bones
        select bones
        swgSuper_UpdateUtilities()
        swgSuper_Log ("Auto rig created " + (bones.count as string) + " bones (" + (if isAnimal then "quadruped" else "biped") + ").")
        true
)

fn swgSuper_BakeAnimation nodes startFrame endFrame = (
        if nodes.count == 0 then (
                messageBox "Select the objects whose animation should be baked." title:"SWG Super Tool"
                false
        ) else (
                local s = startFrame
                local e = endFrame
                if s > e then (
                        local tmp = s
                        s = e
                        e = tmp
                )
                local bakedCount = 0
                with animate on (
                        for f = s to e do (
                                sliderTime = f
                                for obj in nodes do (
                                        if obj != undefined and isValidNode obj then (
                                                at time f (
                                                        obj.position = obj.position
                                                        obj.rotation = obj.rotation
                                                        obj.scale = obj.scale
                                                )
                                                if f == s then bakedCount += 1
                                        )
                                )
                        )
                )
                if bakedCount > 0 then (
                        swgSuper_Log ("Baked animation keys for " + (bakedCount as string) + " nodes between " + (s as string) + " and " + (e as string) + ".")
                        true
                ) else (
                        swgSuper_Log "No valid nodes were baked for animation."
                        false
                )
        )
)

fn swgSuper_SetAnimationRange startFrame endFrame = (
        animationRange = interval (startFrame as integer) (endFrame as integer)
        swgSuper_Log ("Animation range set to " + (startFrame as string) + " - " + (endFrame as string))
)

fn swgSuper_RunImporter = (
        if (swgSuper_IsValidRollout SWG_importRoll) and (isProperty SWG_importRoll #impMGN) then (
                try(
                        pressButton SWG_importRoll.impMGN
                        swgSuper_Log "Importer dialog opened."
                ) catch(
                        messageBox "Unable to trigger the importer UI." title:"SWG Super Tool"
                )
        ) else (
                messageBox "Importer rollout not loaded." title:"SWG Super Tool"
        )
)

fn swgSuper_RunExporter = (
        if (swgSuper_IsValidRollout SWG_exportRoll) and (isProperty SWG_exportRoll #expMSH) then (
                try(
                        pressButton SWG_exportRoll.expMSH
                        swgSuper_Log "Exporter dialog opened."
                ) catch(
                        messageBox "Unable to trigger the exporter UI." title:"SWG Super Tool"
                )
        ) else (
                messageBox "Exporter rollout not loaded." title:"SWG Super Tool"
        )
)

fn swgSuper_RunRoundtrip = (
        local nodes = selection as array
        if swgSuper_PrepareGeometry nodes then (
                if SWG_superPanelControl != undefined then swgSuper_ShowRollout SWG_superPanelControl SWG_exportRoll
                swgSuper_RunExporter()
        )
)

global swgSuper_BatchOpenPath
global swgSuper_BatchSavePath

fn swgSuper_BatchGetOpenFileName caption:"" types:"" = swgSuper_BatchOpenPath
fn swgSuper_BatchGetSaveFileName caption:"" types:"" = swgSuper_BatchSavePath

fn swgSuper_RunBatchRepairMSH = (
        local inputFolder = getSavePath caption:"Select MSH input folder"
        if inputFolder == undefined then (
                swgSuper_Log "Batch repair cancelled (no input folder selected)."
                return false
        )

        local outputFolder = getSavePath caption:"Select MSH output folder"
        if outputFolder == undefined then (
                swgSuper_Log "Batch repair cancelled (no output folder selected)."
                return false
        )

        local files = getFiles (inputFolder + "\\*.msh")
        if files.count == 0 then (
                swgSuper_Log ("Batch repair found no .msh files in " + inputFolder + ".")
                return false
        )

        swgSuper_Log ("Batch repair starting: " + (files.count as string) + " file(s) from " + inputFolder + " to " + outputFolder + ".")

        local originalGetOpenFileName = getOpenFileName
        local originalGetSaveFileName = getSaveFileName
        local successCount = 0
        local failCount = 0

        for f in files do (
                local exportPath = outputFolder + "\\" + (getFileNameFile f) + ".msh"
                local fileSucceeded = false
                try(
                        resetMaxFile #noPrompt

                        swgSuper_BatchOpenPath = f
                        swgSuper_BatchSavePath = exportPath
                        getOpenFileName = swgSuper_BatchGetOpenFileName
                        getSaveFileName = swgSuper_BatchGetSaveFileName

                        if (swgSuper_IsValidRollout SWG_importRoll) and (isProperty SWG_importRoll #impMGN) then (
                                pressButton SWG_importRoll.impMGN
                        ) else (
                                swgSuper_Log ("Batch repair failed to load importer for " + f + ".")
                        )

                        if (swgSuper_IsValidRollout SWG_importRoll) and (isProperty SWG_importRoll #impMGN) then (
                                local importedNodes = for n in geometry collect n
                                if importedNodes.count == 0 then (
                                        swgSuper_Log ("Batch repair skipped (no geometry imported) for " + f + ".")
                                ) else if swgSuper_PrepareGeometry importedNodes then (
                                        if SWG_superPanelControl != undefined then swgSuper_ShowRollout SWG_superPanelControl SWG_exportRoll
                                        if (swgSuper_IsValidRollout SWG_exportRoll) and (isProperty SWG_exportRoll #expMSH) then (
                                                pressButton SWG_exportRoll.expMSH
                                                swgSuper_Log ("Batch repair succeeded: " + (getFileNameFile f) + " -> " + exportPath)
                                                fileSucceeded = true
                                        ) else (
                                                swgSuper_Log ("Batch repair failed to load exporter for " + f + ".")
                                        )
                                ) else (
                                        swgSuper_Log ("Batch repair skipped (no valid geometry) for " + f + ".")
                                )
                        )
                ) catch(
                        swgSuper_Log ("Batch repair failed: " + f + ".")
                )

                getOpenFileName = originalGetOpenFileName
                getSaveFileName = originalGetSaveFileName

                if fileSucceeded then (
                        successCount += 1
                ) else (
                        failCount += 1
                )
        )

        swgSuper_Log ("Batch repair complete. Success: " + (successCount as string) + ", failed: " + (failCount as string) + ".")
        true
)

fn swgSuper_ShowStandaloneImporter = (
        if SWG_importFloater != undefined then (
                try(closeRolloutFloater SWG_importFloater) catch()
        )
        SWG_importFloater = newRolloutFloater "SWG Importer" 220 190 50 50
        addRollout SWG_importRoll SWG_importFloater
)

fn swgSuper_ShowStandaloneExporter = (
        if SWG_exportFloater != undefined then (
                try(closeRolloutFloater SWG_exportFloater) catch()
        )
        SWG_exportFloater = newRolloutFloater "SWG Exporter" 420 640 60 60
        addRollout SWG_exportRoll SWG_exportFloater
)

fn swgSuper_AddAnimationClip clipName startFrame endFrame = (
        local trimmedName = if clipName == undefined then "" else trimRight (trimLeft clipName)
        if trimmedName == "" then (
                messageBox "Provide a name for the animation clip." title:"SWG Super Tool"
        ) else (
                if SWG_superAnimationLibrary == undefined or classOf SWG_superAnimationLibrary != Array do (
                        SWG_superAnimationLibrary = #()
                )
                local record = #(trimmedName, startFrame as integer, endFrame as integer)
                SWG_superAnimationLibrary[SWG_superAnimationLibrary.count + 1] = record
                swgSuper_Log ("Registered animation clip '" + trimmedName + "'.")
                swgSuper_UpdateAnimationList()
        )
)

fn swgSuper_BakeSelectedClips indices = (
        if indices.count == 0 then (
                messageBox "Select one or more clips from the list to bake." title:"SWG Super Tool"
        ) else (
                local nodes = selection as array
                if nodes.count == 0 then (
                        messageBox "Select scene nodes to bake before running this action." title:"SWG Super Tool"
                ) else (
                        for idx in indices do (
                                if idx >= 1 and idx <= SWG_superAnimationLibrary.count then (
                                        local clip = SWG_superAnimationLibrary[idx]
                                        swgSuper_BakeAnimation nodes clip[2] clip[3]
                                )
                        )
                )
        )
)

fn swgSuper_GuidedMeshSelectionCount = (
        local count = 0
        for n in selection do (
                if n != undefined and isValidNode n and (superClassOf n == GeometryClass) do count += 1
        )
        count
)

fn swgSuper_GuidedSkeletonRootValid = (
        SWG_superSkeletonRoot != undefined and isValidNode SWG_superSkeletonRoot
)

fn swgSuper_GuidedStepTitle stepIndex = (
        case stepIndex of (
                1: "Select Mesh"
                2: "Choose Rig Type"
                3: "Auto Rig"
                4: "Verify Root"
                5: "Prep Skeleton"
                6: "Bake Test Clip"
                default: "Guided Rigging"
        )
)

fn swgSuper_GuidedStepHint stepIndex = (
        case stepIndex of (
                1: (
                        if swgSuper_GuidedMeshSelectionCount() == 0 then (
                                "Select one or more mesh nodes to continue."
                        ) else (
                                "Mesh selection detected."
                        )
                )
                2: "Choose Auto, Biped, or Quadruped before continuing."
                3: (
                        if swgSuper_GuidedMeshSelectionCount() == 0 then (
                                "Select mesh nodes to auto-rig."
                        ) else (
                                "Click Next to run auto-rigging for the chosen type."
                        )
                )
                4: (
                        if swgSuper_GuidedSkeletonRootValid() then (
                                "Skeleton root set to " + SWG_superSkeletonRoot.name + "."
                        ) else (
                                "No skeleton root found; click 'Use Selection as Skeleton Root' or run Auto Rig."
                        )
                )
                5: (
                        if swgSuper_GuidedSkeletonRootValid() then (
                                "Click Next to prep skeleton bones."
                        ) else (
                                "Set a skeleton root before prepping bones."
                        )
                )
                6: (
                        if swgSuper_GuidedSkeletonRootValid() then (
                                "Click Finish to bake a test clip across the current animation range."
                        ) else (
                                "Set a skeleton root before baking a test clip."
                        )
                )
                default: ""
        )
)

fn swgSuper_UpdateGuidedRiggingUI = (
        if SWG_superGuidedRigging != undefined do (
                local title = swgSuper_GuidedStepTitle SWG_superGuidedStep
                SWG_superGuidedRigging.lblStep.text = "Step " + (SWG_superGuidedStep as string) + " of 6: " + title
                SWG_superGuidedRigging.lblHint.text = swgSuper_GuidedStepHint SWG_superGuidedStep
                SWG_superGuidedRigging.btnBack.enabled = (SWG_superGuidedStep > 1)
                SWG_superGuidedRigging.btnNext.text = if SWG_superGuidedStep >= 6 then "Finish" else "Next"
                SWG_superGuidedRigging.ddlRigType.selection = findItem SWG_superGuidedRigging.ddlRigType.items SWG_superGuidedRigType
        )
)

fn swgSuper_GuidedNext = (
        local canAdvance = true
        case SWG_superGuidedStep of (
                1: swgSuper_AutoConfigureFromScene()
                2: (
                        if SWG_superGuidedRigging != undefined do (
                                SWG_superGuidedRigType = SWG_superGuidedRigging.ddlRigType.items[SWG_superGuidedRigging.ddlRigType.selection]
                        )
                )
                3: swgSuper_AutoRigSkeleton rigType:SWG_superGuidedRigType
                4: (
                        if not swgSuper_GuidedSkeletonRootValid() then (
                                messageBox "No skeleton root detected yet. Select a root bone or run Auto Rig." title:"SWG Super Tool"
                                canAdvance = false
                        ) else (
                                swgSuper_Log ("Skeleton root verified: " + SWG_superSkeletonRoot.name)
                        )
                )
                5: (
                        if swgSuper_GuidedSkeletonRootValid() then (
                                swgSuper_PrepareSkeleton (swgSuper_GatherSkeletonNodes SWG_superSkeletonRoot)
                        ) else (
                                messageBox "Set a skeleton root before prepping the skeleton." title:"SWG Super Tool"
                                canAdvance = false
                        )
                )
                6: (
                        if swgSuper_GuidedSkeletonRootValid() then (
                                if swgSuper_ValidateSkeleton assetType:SWG_superGuidedRigType promptOnFailure:true then (
                                        swgSuper_BakeAnimation (swgSuper_GatherSkeletonNodes SWG_superSkeletonRoot) animationRange.start.frame animationRange.end.frame
                                ) else (
                                        canAdvance = false
                                )
                        ) else (
                                messageBox "Set a skeleton root before baking a test clip." title:"SWG Super Tool"
                                canAdvance = false
                        )
                )
        )

        if canAdvance then (
                if SWG_superGuidedStep < 6 then SWG_superGuidedStep += 1
                swgSuper_UpdateGuidedRiggingUI()
        )
)

fn swgSuper_GuidedBack = (
        if SWG_superGuidedStep > 1 then SWG_superGuidedStep -= 1
        swgSuper_UpdateGuidedRiggingUI()
)

fn swgSuper_ShowGuidedRigging = (
        if SWG_superGuidedFloater != undefined then (
                try(closeRolloutFloater SWG_superGuidedFloater) catch()
        )
        SWG_superGuidedStep = 1
        SWG_superGuidedFloater = newRolloutFloater "Guided Rigging" 420 480 80 80
        addRollout SWG_superGuidedRigging SWG_superGuidedFloater
        swgSuper_UpdateGuidedRiggingUI()
)

rollout SWG_superGuidedRigging "Guided Rigging" width:400 height:440 (
        label lblChecklist "1. Select mesh\n2. Choose rig type\n3. Auto-rig\n4. Verify root\n5. Prep skeleton\n6. Bake test clip" width:360 height:70 align:#left
        label lblStep "Step 1 of 6: Select Mesh" width:360 align:#left
        dropdownlist ddlRigType "Rig Type" items:#("Auto", "Biped", "Quadruped") width:160
        button btnUseSelectionRoot "Use Selection as Skeleton Root" width:220
        label lblHint "" width:360 height:60 align:#left
        group "Controls" (
                button btnBack "Back" width:100 across:2
                button btnNext "Next" width:100
        )
        on SWG_superGuidedRigging open do (
                swgSuper_UpdateGuidedRiggingUI()
        )
        on ddlRigType selected idx do (
                if idx >= 1 and idx <= ddlRigType.items.count then SWG_superGuidedRigType = ddlRigType.items[idx]
                swgSuper_UpdateGuidedRiggingUI()
        )
        on btnUseSelectionRoot pressed do (
                if selection.count == 1 then (
                        swgSuper_SetSkeletonRoot selection[1]
                        swgSuper_UpdateGuidedRiggingUI()
                ) else (
                        messageBox "Select exactly one root bone." title:"SWG Super Tool"
                )
        )
        on btnBack pressed do swgSuper_GuidedBack()
        on btnNext pressed do swgSuper_GuidedNext()
)

rollout SWG_superUtilities "Utilities" width:380 height:360 (
        group "Content Paths" (
                label lblMGNPath "MGN: <asset or output path not set>" width:360
                label lblMSHPath "MSH: <asset or output path not set>" width:360
                label lblSKTPath "SKT: <asset or output path not set>" width:360
                button btnCopyMGN "Copy MGN Path" width:110 across:3
                button btnCopyMSH "Copy MSH Path" width:110
                button btnCopySKT "Copy SKT Path" width:110
                button btnOpenFolder "Open Output Folder" width:150 align:#left
        )
        group "Automation" (
                button btnImport "Import Mesh" width:110 across:3
                button btnExport "Export Mesh" width:110
                button btnRoundtrip "Full Roundtrip" width:130
                button btnPrepGeo "Prep Geometry" width:120 across:2
                button btnPrepSkeleton "Prep Skeleton" width:120
                button btnAutoRig "Auto Rig Skeleton" width:150
                button btnBatchRepair "Batch Repair MSH" width:150
                button btnLegacyImporter "Legacy Importer" width:120 across:2
                button btnLegacyExporter "Legacy Exporter" width:120
        )
        group "Mesh Cleanup" (
                checkbox chkAutoFixMeshQuality "Auto Fix Mesh Quality" checked:SWG_superAutoFixMeshQuality width:200
                spinner spnWeldThreshold "Weld Threshold" type:#float range:[0,100,SWG_superAutoFixWeldThreshold] width:160 align:#left
                checkbox chkSmoothAngleAuto "Auto Smooth Angle" checked:(SWG_superAutoFixSmoothAngle < 0) width:200
                spinner spnSmoothAngle "Smooth Angle" type:#float range:[0,180,SWG_superAutoFixSmoothAngle] width:160 align:#left
        )
        group "Skeleton & Animations" (
                label lblSkeletonRoot "Skeleton Root: <not set>" width:360
                button btnSetSkeletonRoot "Use Selection as Skeleton Root" width:200
                editText edtClipName "Clip Name" text:"" width:180 align:#left across:3
                spinner spnClipStart "Start" type:#integer range:[-100000,100000,animationRange.start.frame as integer] align:#left width:80
                spinner spnClipEnd "End" type:#integer range:[-100000,100000,animationRange.end.frame as integer] align:#left width:80
                button btnRegisterClip "Register Clip" width:120 align:#left
                multiListBox lbAnimations "Clips" items:#() height:5 width:350
                button btnBakeClip "Bake Selected Clips" width:160 align:#left
                button btnApplyClipRange "Apply Range From Clip" width:180 align:#right
        )
        on btnCopyMGN pressed do (
                setClipBoardText (swgSuper_GetOutputPath "mgn")
                swgSuper_Log "MGN path copied to clipboard."
        )
        on btnCopyMSH pressed do (
                setClipBoardText (swgSuper_GetOutputPath "msh")
                swgSuper_Log "MSH path copied to clipboard."
        )
        on btnCopySKT pressed do (
                setClipBoardText (swgSuper_GetOutputPath "skt")
                swgSuper_Log "SKT path copied to clipboard."
        )
        on btnOpenFolder pressed do (
                if (SWG_superOutputRoot != undefined and SWG_superOutputRoot != "") then (
                        shellLaunch SWG_superOutputRoot ""
                ) else (
                        messageBox "Set an output folder before opening it." title:"SWG Super Tool"
                )
        )
        on btnImport pressed do swgSuper_RunImporter()
        on btnExport pressed do swgSuper_RunExporter()
        on btnRoundtrip pressed do swgSuper_RunRoundtrip()
        on btnPrepGeo pressed do swgSuper_PrepareGeometry (selection as array)
        on btnPrepSkeleton pressed do swgSuper_PrepareSkeleton (selection as array)
        on btnAutoRig pressed do swgSuper_AutoRigSkeleton()
        on btnBatchRepair pressed do swgSuper_RunBatchRepairMSH()
        on btnLegacyImporter pressed do swgSuper_ShowStandaloneImporter()
        on btnLegacyExporter pressed do swgSuper_ShowStandaloneExporter()
        on chkAutoFixMeshQuality changed state do (
                SWG_superAutoFixMeshQuality = state
                swgSuper_Log ("Auto Fix Mesh Quality " + (if state then "enabled" else "disabled") + ".")
        )
        on spnWeldThreshold changed val do (
                SWG_superAutoFixWeldThreshold = val
                swgSuper_Log ("Auto Fix weld threshold set to " + (val as string) + ".")
        )
        on spnSmoothAngle changed val do (
                if chkSmoothAngleAuto.checked == false then (
                        SWG_superAutoFixSmoothAngle = val
                        swgSuper_Log ("Auto Fix smoothing angle set to " + (val as string) + ".")
                )
        )
        on chkSmoothAngleAuto changed state do (
                spnSmoothAngle.enabled = (state == false)
                if state then (
                        SWG_superAutoFixSmoothAngle = -1
                        swgSuper_Log "Auto Fix smoothing angle set to Auto (computed per mesh)."
                ) else (
                        SWG_superAutoFixSmoothAngle = spnSmoothAngle.value
                        swgSuper_Log ("Auto Fix smoothing angle set to " + (spnSmoothAngle.value as string) + ".")
                )
        )
        on btnSetSkeletonRoot pressed do (
                if selection.count == 1 then (
                        swgSuper_SetSkeletonRoot selection[1]
                ) else (
                        messageBox "Select exactly one root bone." title:"SWG Super Tool"
                )
        )
        on btnRegisterClip pressed do (
                swgSuper_AddAnimationClip edtClipName.text spnClipStart.value spnClipEnd.value
        )
        on btnBakeClip pressed do (
                swgSuper_BakeSelectedClips (lbAnimations.selection as array)
        )
        on btnApplyClipRange pressed do (
                if lbAnimations.selection.count > 0 then (
                        local idx = lbAnimations.selection[1]
                        if idx >= 1 and idx <= SWG_superAnimationLibrary.count then (
                                local clip = SWG_superAnimationLibrary[idx]
                                swgSuper_SetAnimationRange clip[2] clip[3]
                        )
                ) else (
                        messageBox "Select a clip to apply its range." title:"SWG Super Tool"
                )
        )
        on lbAnimations doubleClicked idx do (
                if idx >= 1 and idx <= SWG_superAnimationLibrary.count then (
                        local clip = SWG_superAnimationLibrary[idx]
                        swgSuper_SetAnimationRange clip[2] clip[3]
                )
        )
        on SWG_superUtilities open do (
                chkAutoFixMeshQuality.checked = (SWG_superAutoFixMeshQuality == true)
                spnWeldThreshold.value = SWG_superAutoFixWeldThreshold
                if SWG_superAutoFixSmoothAngle < 0 then (
                        chkSmoothAngleAuto.checked = true
                        spnSmoothAngle.enabled = false
                        spnSmoothAngle.value = 45.0
                ) else (
                        chkSmoothAngleAuto.checked = false
                        spnSmoothAngle.enabled = true
                        spnSmoothAngle.value = SWG_superAutoFixSmoothAngle
                )
                swgSuper_UpdateUtilities()
        )
)

rollout SWG_superDashboard "SWG Content Super Tool" width:420 height:660 (
        label lblTitle "Star Wars Galaxies Content Suite" align:#center
        label lblSubtitle "Importer, exporter, and animation workflow manager" align:#center
        group "Asset" (
                editText edtAssetName "Asset Tag" text:SWG_superAssetName width:240 align:#left
                button btnBrowseOutput "Select Output Folder" width:160 align:#right
                button btnAutoAsset "Smart Configure" width:160 align:#center
                label lblOutput "Output: <not set>" width:360
        )
        group "Automation" (
                button btnPrepGeometry "Prepare Geometry" width:160 across:2
                button btnPrepBones "Prepare Skeleton" width:160
                button btnAutoRig "Auto Rig Skeleton" width:160
                button btnBakeAnim "Bake Animation" width:160 across:2
                button btnRoundtrip "Roundtrip Export" width:160
                button btnSmartExport "Smart Export" width:160 align:#center
                button btnGuidedRigging "Guided Rigging" width:160 align:#center
        )
        group "Animation Range" (
                spinner spnStart "Start" type:#integer range:[-100000,100000,animationRange.start.frame as integer] align:#left across:2
                spinner spnEnd "End" type:#integer range:[-100000,100000,animationRange.end.frame as integer]
                button btnApplyRange "Apply Range" width:140
        )
        group "Panels" (
                subRollout srPanels "" width:380 height:360
                button btnShowImporter "Importer" width:120 across:3
                button btnShowExporter "Exporter" width:120
                button btnShowUtilities "Utilities" width:120
        )
        group "Log" (
                label lblStatus "Status: Ready" width:360
                -- Use a dotNet TextBox for multiline/readonly logging (MAXScript has no multilineEditText control).
                dotNetControl logOutputDotNet "System.Windows.Forms.TextBox" width:360 height:120
                editText logOutputFallback "" width:360 height:120 readOnly:true
        )

        on SWG_superDashboard open do (
                SWG_superPanelControl = srPanels
                if SWG_superAssetName != undefined then edtAssetName.text = SWG_superAssetName
                local hasDotNetLog = false
                if isProperty SWG_superDashboard #logOutputDotNet do (
                        try(
                                local scrollBars = dotNetClass "System.Windows.Forms.ScrollBars"
                                SWG_superDashboard.logOutputDotNet.Multiline = true
                                SWG_superDashboard.logOutputDotNet.ReadOnly = true
                                SWG_superDashboard.logOutputDotNet.ScrollBars = scrollBars.Vertical
                                SWG_superDashboard.logOutputDotNet.WordWrap = false
                                hasDotNetLog = true
                        ) catch(hasDotNetLog = false)
                )
                if hasDotNetLog then (
                        SWG_superDashboard.logOutputDotNet.visible = true
                        SWG_superDashboard.logOutputFallback.visible = false
                ) else (
                        if isProperty SWG_superDashboard #logOutputDotNet do SWG_superDashboard.logOutputDotNet.visible = false
                        SWG_superDashboard.logOutputFallback.visible = true
                )
                if SWG_superOutputRoot != undefined and SWG_superOutputRoot != "" then (
                        lblOutput.text = "Output: " + SWG_superOutputRoot
                ) else (
                        lblOutput.text = "Output: <not set>"
                )
                swgSuper_ShowRollout srPanels SWG_importRoll
                swgSuper_Log "Ready"
        )
        on btnBrowseOutput pressed do (
                local path = getSavePath caption:"Choose SWG export root"
                if path != undefined do (
                        swgSuper_SetOutputRoot path
                        swgSuper_Log ("Output folder set to " + path)
                )
        )
        on edtAssetName entered text do (
                local sanitized = swgSuper_SetAssetName text
                swgSuper_Log ("Asset tag set to " + sanitized)
        )
        on btnPrepGeometry pressed do (
                swgSuper_PrepareGeometry (selection as array)
        )
        on btnPrepBones pressed do (
                swgSuper_PrepareSkeleton (selection as array)
        )
        on btnAutoRig pressed do (
                swgSuper_AutoRigSkeleton()
        )
        on btnBakeAnim pressed do (
                swgSuper_BakeAnimation (selection as array) spnStart.value spnEnd.value
        )
        on btnRoundtrip pressed do swgSuper_RunRoundtrip()
        on btnAutoAsset pressed do (
                swgSuper_AutoConfigureFromScene()
        )
        on btnSmartExport pressed do (
                swgSuper_RunExportWizard true
        )
        on btnGuidedRigging pressed do (
                swgSuper_ShowGuidedRigging()
        )
        on btnApplyRange pressed do (
                swgSuper_SetAnimationRange spnStart.value spnEnd.value
        )
        on btnShowImporter pressed do (
                swgSuper_ShowRollout srPanels SWG_importRoll
                swgSuper_Log "Importer ready"
        )
        on btnShowExporter pressed do (
                swgSuper_ShowRollout srPanels SWG_exportRoll
                swgSuper_Log "Exporter ready"
        )
        on btnShowUtilities pressed do (
                swgSuper_ShowRollout srPanels SWG_superUtilities
                swgSuper_Log "Utilities ready"
        )
)

if SWG_superVersionSupported then (
        if SWG_SuperToolFloater != undefined then (
                try(closeRolloutFloater SWG_SuperToolFloater) catch()
        )
        SWG_SuperToolFloater = newRolloutFloater "SWG Content Super Tool" 440 720 40 40
        addRollout SWG_superDashboard SWG_SuperToolFloater

        swgSuper_UpdateUtilities()
)

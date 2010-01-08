BamConversionMain.cpp                                                                               0000664 0001074 0001074 00000001656 11307517135 015027  0                                                                                                    ustar   barnett                         barnett                                                                                                                                                                                                                #include <iostream>
#include "BamReader.h"
#include "BamWriter.h"
using namespace BamTools;
using namespace std;

int main(int argc, char* argv[]) {

	if(argc != 3) {
		cout << "USAGE: " << argv[0] << " <input BAM file> <output BAM file>" << endl;
		exit(1);
	}

	// localize our arguments
	const char* inputFilename  = argv[1];
	const char* outputFilename = argv[2];

	// open our BAM reader
	BamReader reader;
	reader.Open(inputFilename);

	// retrieve the SAM header text
	string samHeader = reader.GetHeaderText();

	// retrieve the reference sequence vector
	RefVector referenceSequences = reader.GetReferenceData();

	// open the BAM writer
	BamWriter writer;
	writer.Open(outputFilename, samHeader, referenceSequences);

	// copy all of the reads from the input file to the output file
	BamAlignment al;
	while(reader.GetNextAlignment(al)) writer.SaveAlignment(al);

	// close our files
	reader.Close();
	writer.Close();

	return 0;
}
                                                                                  BamDumpMain.cpp                                                                                     0000664 0001074 0001074 00000003172 11307517135 013602  0                                                                                                    ustar   barnett                         barnett                                                                                                                                                                                                                // ***************************************************************************
// BamDump.cpp (c) 2009 Derek Barnett
// Marth Lab, Department of Biology, Boston College
// All rights reserved.
// ---------------------------------------------------------------------------
// Last modified: 15 July 2009 (DB)
// ---------------------------------------------------------------------------
// Spits out all alignments in BAM file.
//
// N.B. - Could result in HUGE text file. This is mostly a debugging tool
// for small files.  You have been warned.
// ***************************************************************************

// Std C/C++ includes
#include <cstdlib>
#include <iostream>
#include <string>
using namespace std;

// BamTools includes
#include "BamReader.h"
#include "BamWriter.h"
using namespace BamTools;

void PrintAlignment(const BamAlignment&);

int main(int argc, char* argv[]) {

	// validate argument count
	if( argc != 2 ) {
		cerr << "USAGE: " << argv[0] << " <input BAM file> " << endl;
		exit(1);
	}

	string filename = argv[1];
	cout << "Printing alignments from file: " << filename << endl;
	
	BamReader reader;
	reader.Open(filename);
	
	BamAlignment bAlignment;
	while (reader.GetNextAlignment(bAlignment)) {
		PrintAlignment(bAlignment);
	}

	reader.Close();
	return 0;
}
	
// Spit out basic BamAlignment data 
void PrintAlignment(const BamAlignment& alignment) {
	cout << "---------------------------------" << endl;
	cout << "Name: "       << alignment.Name << endl;
	cout << "Aligned to: " << alignment.RefID;
	cout << ":"            << alignment.Position << endl;
}
                                                                                                                                                                                                                                                                                                                                                                                                      BamReader.cpp                                                                                       0000664 0001074 0001074 00000107374 11307521263 013300  0                                                                                                    ustar   barnett                         barnett                                                                                                                                                                                                                // ***************************************************************************
// BamReader.cpp (c) 2009 Derek Barnett, Michael Str�mberg
// Marth Lab, Department of Biology, Boston College
// All rights reserved.
// ---------------------------------------------------------------------------
// Last modified: 8 December 2009 (DB)
// ---------------------------------------------------------------------------
// Uses BGZF routines were adapted from the bgzf.c code developed at the Broad
// Institute.
// ---------------------------------------------------------------------------
// Provides the basic functionality for reading BAM files
// ***************************************************************************

// C++ includes
#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

// BamTools includes
#include "BGZF.h"
#include "BamReader.h"
using namespace BamTools;
using namespace std;

struct BamReader::BamReaderPrivate {

    // -------------------------------
    // data members
    // -------------------------------

    // general data
    BgzfData  mBGZF;
    string    HeaderText;
    BamIndex  Index;
    RefVector References;
    bool      IsIndexLoaded;
    int64_t   AlignmentsBeginOffset;
    string    Filename;
    string    IndexFilename;

    // user-specified region values
    bool IsRegionSpecified;
    int  CurrentRefID;
    int  CurrentLeft;

    // BAM character constants
    const char* DNA_LOOKUP;
    const char* CIGAR_LOOKUP;

    // -------------------------------
    // constructor & destructor
    // -------------------------------
    BamReaderPrivate(void);
    ~BamReaderPrivate(void);

    // -------------------------------
    // "public" interface
    // -------------------------------

    // flie operations
    void Close(void);
    bool Jump(int refID, int position = 0);
    void Open(const string& filename, const string& indexFilename = "");
    bool Rewind(void);

    // access alignment data
    bool GetNextAlignment(BamAlignment& bAlignment);

    // access auxiliary data
    const string GetHeaderText(void) const;
    const int GetReferenceCount(void) const;
    const RefVector GetReferenceData(void) const;
    const int GetReferenceID(const string& refName) const;

    // index operations
    bool CreateIndex(void);

    // -------------------------------
    // internal methods
    // -------------------------------

    // *** reading alignments and auxiliary data *** //

    // calculate bins that overlap region ( left to reference end for now )
    int BinsFromRegion(int refID, int left, uint16_t[MAX_BIN]);
    // calculates alignment end position based on starting position and provided CIGAR operations
    int CalculateAlignmentEnd(const int& position, const std::vector<CigarOp>& cigarData);
    // calculate file offset for first alignment chunk overlapping 'left'
    int64_t GetOffset(int refID, int left);
    // checks to see if alignment overlaps current region
    bool IsOverlap(BamAlignment& bAlignment);
    // retrieves header text from BAM file
    void LoadHeaderData(void);
    // retrieves BAM alignment under file pointer
    bool LoadNextAlignment(BamAlignment& bAlignment);
    // builds reference data structure from BAM file
    void LoadReferenceData(void);

    // *** index file handling *** //

    // calculates index for BAM file
    bool BuildIndex(void);
    // clear out inernal index data structure
    void ClearIndex(void);
    // saves BAM bin entry for index
    void InsertBinEntry(BamBinMap& binMap, const uint32_t& saveBin, const uint64_t& saveOffset, const uint64_t& lastOffset);
    // saves linear offset entry for index
    void InsertLinearOffset(LinearOffsetVector& offsets, const BamAlignment& bAlignment, const uint64_t& lastOffset);
    // loads index from BAM index file
    bool LoadIndex(void);
    // simplifies index by merging 'chunks'
    void MergeChunks(void);
    // round-up 32-bit integer to next power-of-2
    void Roundup32(int& value);
    // saves index to BAM index file
    bool WriteIndex(void);
};

// -----------------------------------------------------
// BamReader implementation (wrapper around BRPrivate)
// -----------------------------------------------------

// constructor
BamReader::BamReader(void) {
    d = new BamReaderPrivate;
}

// destructor
BamReader::~BamReader(void) {
    delete d;
    d = 0;
}

// file operations
void BamReader::Close(void) { d->Close(); }
bool BamReader::Jump(int refID, int position) { return d->Jump(refID, position); }
void BamReader::Open(const string& filename, const string& indexFilename) { d->Open(filename, indexFilename); }
bool BamReader::Rewind(void) { return d->Rewind(); }

// access alignment data
bool BamReader::GetNextAlignment(BamAlignment& bAlignment) { return d->GetNextAlignment(bAlignment); }

// access auxiliary data
const string    BamReader::GetHeaderText(void) const { return d->HeaderText; }
const int       BamReader::GetReferenceCount(void) const { return d->References.size(); }
const RefVector BamReader::GetReferenceData(void) const { return d->References; }
const int       BamReader::GetReferenceID(const string& refName) const { return d->GetReferenceID(refName); }

// index operations
bool BamReader::CreateIndex(void) { return d->CreateIndex(); }

// -----------------------------------------------------
// BamReaderPrivate implementation
// -----------------------------------------------------

// constructor
BamReader::BamReaderPrivate::BamReaderPrivate(void)
    : IsIndexLoaded(false)
    , AlignmentsBeginOffset(0)
    , IsRegionSpecified(false)
    , CurrentRefID(0)
    , CurrentLeft(0)
    , DNA_LOOKUP("=ACMGRSVTWYHKDBN")
    , CIGAR_LOOKUP("MIDNSHP")
{ }

// destructor
BamReader::BamReaderPrivate::~BamReaderPrivate(void) {
    Close();
}

// calculate bins that overlap region ( left to reference end for now )
int BamReader::BamReaderPrivate::BinsFromRegion(int refID, int left, uint16_t list[MAX_BIN]) {

    // get region boundaries
    uint32_t begin = (unsigned int)left;
    uint32_t end   = (unsigned int)References.at(refID).RefLength - 1;

    // initialize list, bin '0' always a valid bin
    int i = 0;
    list[i++] = 0;

    // get rest of bins that contain this region
    unsigned int k;
    for (k =    1 + (begin>>26); k <=    1 + (end>>26); ++k) { list[i++] = k; }
    for (k =    9 + (begin>>23); k <=    9 + (end>>23); ++k) { list[i++] = k; }
    for (k =   73 + (begin>>20); k <=   73 + (end>>20); ++k) { list[i++] = k; }
    for (k =  585 + (begin>>17); k <=  585 + (end>>17); ++k) { list[i++] = k; }
    for (k = 4681 + (begin>>14); k <= 4681 + (end>>14); ++k) { list[i++] = k; }

    // return number of bins stored
    return i;
}

// populates BAM index data structure from BAM file data
bool BamReader::BamReaderPrivate::BuildIndex(void) {

    // check to be sure file is open
    if (!mBGZF.IsOpen) { return false; }

    // move file pointer to beginning of alignments
    Rewind();

    // get reference count, reserve index space
    int numReferences = References.size();
    for ( int i = 0; i < numReferences; ++i ) {
        Index.push_back(ReferenceIndex());
    }

    // sets default constant for bin, ID, offset, coordinate variables
    const uint32_t defaultValue = 0xffffffffu;

    // bin data
    uint32_t saveBin(defaultValue);
    uint32_t lastBin(defaultValue);

    // reference ID data
    int32_t saveRefID(defaultValue);
    int32_t lastRefID(defaultValue);

    // offset data
    uint64_t saveOffset = mBGZF.Tell();
    uint64_t lastOffset = saveOffset;

    // coordinate data
    int32_t lastCoordinate = defaultValue;

    BamAlignment bAlignment;
    while( GetNextAlignment(bAlignment) ) {

        // change of chromosome, save ID, reset bin
        if ( lastRefID != bAlignment.RefID ) {
            lastRefID = bAlignment.RefID;
            lastBin   = defaultValue;
        }

        // if lastCoordinate greater than BAM position - file not sorted properly
        else if ( lastCoordinate > bAlignment.Position ) {
            printf("BAM file not properly sorted:\n");
            printf("Alignment %s : %d > %d on reference (id = %d)", bAlignment.Name.c_str(), lastCoordinate, bAlignment.Position, bAlignment.RefID);
            exit(1);
        }

        // if valid reference && BAM bin spans some minimum cutoff (smaller bin ids span larger regions)
        if ( (bAlignment.RefID >= 0) && (bAlignment.Bin < 4681) ) {

            // save linear offset entry (matched to BAM entry refID)
            ReferenceIndex& refIndex = Index.at(bAlignment.RefID);
            LinearOffsetVector& offsets = refIndex.Offsets;
            InsertLinearOffset(offsets, bAlignment, lastOffset);
        }

        // if current BamAlignment bin != lastBin, "then possibly write the binning index"
        if ( bAlignment.Bin != lastBin ) {

            // if not first time through
            if ( saveBin != defaultValue ) {

                // save Bam bin entry
                ReferenceIndex& refIndex = Index.at(saveRefID);
                BamBinMap& binMap = refIndex.Bins;
                InsertBinEntry(binMap, saveBin, saveOffset, lastOffset);
            }

            // update saveOffset
            saveOffset = lastOffset;

            // update bin values
            saveBin = bAlignment.Bin;
            lastBin = bAlignment.Bin;

            // update saveRefID
            saveRefID = bAlignment.RefID;

            // if invalid RefID, break out (why?)
            if ( saveRefID < 0 ) { break; }
        }

        // make sure that current file pointer is beyond lastOffset
        if ( mBGZF.Tell() <= (int64_t)lastOffset  ) {
            printf("Error in BGZF offsets.\n");
            exit(1);
        }

        // update lastOffset
        lastOffset = mBGZF.Tell();

        // update lastCoordinate
        lastCoordinate = bAlignment.Position;
    }

    // save any leftover BAM data (as long as refID is valid)
    if ( saveRefID >= 0 ) {
        // save Bam bin entry
        ReferenceIndex& refIndex = Index.at(saveRefID);
        BamBinMap& binMap = refIndex.Bins;
        InsertBinEntry(binMap, saveBin, saveOffset, lastOffset);
    }

    // simplify index by merging chunks
    MergeChunks();

    // iterate over references
    BamIndex::iterator indexIter = Index.begin();
    BamIndex::iterator indexEnd  = Index.end();
    for ( int i = 0; indexIter != indexEnd; ++indexIter, ++i ) {

        // get reference index data
        ReferenceIndex& refIndex = (*indexIter);
        BamBinMap& binMap = refIndex.Bins;
        LinearOffsetVector& offsets = refIndex.Offsets;

        // store whether reference has alignments or no
        References[i].RefHasAlignments = ( binMap.size() > 0 );

        // sort linear offsets
        sort(offsets.begin(), offsets.end());
    }


    // rewind file pointer to beginning of alignments, return success/fail
    return Rewind();
}

// calculates alignment end position based on starting position and provided CIGAR operations
int BamReader::BamReaderPrivate::CalculateAlignmentEnd(const int& position, const vector<CigarOp>& cigarData) {

    // initialize alignment end to starting position
    int alignEnd = position;

    // iterate over cigar operations
    vector<CigarOp>::const_iterator cigarIter = cigarData.begin();
    vector<CigarOp>::const_iterator cigarEnd  = cigarData.end();
    for ( ; cigarIter != cigarEnd; ++cigarIter) {
        char cigarType = (*cigarIter).Type;
        if ( cigarType == 'M' || cigarType == 'D' || cigarType == 'N' ) {
            alignEnd += (*cigarIter).Length;
        }
    }
    return alignEnd;
}


// clear index data structure
void BamReader::BamReaderPrivate::ClearIndex(void) {
    Index.clear(); // sufficient ??
}

// closes the BAM file
void BamReader::BamReaderPrivate::Close(void) {
    mBGZF.Close();
    ClearIndex();
    HeaderText.clear();
    IsRegionSpecified = false;
}

// create BAM index from BAM file (keep structure in memory) and write to default index output file
bool BamReader::BamReaderPrivate::CreateIndex(void) {

    // clear out index
    ClearIndex();

	// build (& save) index from BAM file
    bool ok = true;
    ok &= BuildIndex();
    ok &= WriteIndex();

	// return success/fail
    return ok;
}

// returns RefID for given RefName (returns References.size() if not found)
const int BamReader::BamReaderPrivate::GetReferenceID(const string& refName) const {

    // retrieve names from reference data
    vector<string> refNames;
    RefVector::const_iterator refIter = References.begin();
    RefVector::const_iterator refEnd  = References.end();
    for ( ; refIter != refEnd; ++refIter) {
        refNames.push_back( (*refIter).RefName );
    }

    // return 'index-of' refName ( if not found, returns refNames.size() )
    return distance(refNames.begin(), find(refNames.begin(), refNames.end(), refName));
}

// get next alignment (from specified region, if given)
bool BamReader::BamReaderPrivate::GetNextAlignment(BamAlignment& bAlignment) {

    // if valid alignment available
    if ( LoadNextAlignment(bAlignment) ) {

        // if region not specified, return success
        if ( !IsRegionSpecified ) { return true; }

        // load next alignment until region overlap is found
        while ( !IsOverlap(bAlignment) ) {
            // if no valid alignment available (likely EOF) return failure
            if ( !LoadNextAlignment(bAlignment) ) { return false; }
        }

        // return success (alignment found that overlaps region)
        return true;
    }

    // no valid alignment
    else { return false; }
}

// calculate closest indexed file offset for region specified
int64_t BamReader::BamReaderPrivate::GetOffset(int refID, int left) {

    // calculate which bins overlap this region
    uint16_t* bins = (uint16_t*)calloc(MAX_BIN, 2);
    int numBins = BinsFromRegion(refID, left, bins);

    // get bins for this reference
    const ReferenceIndex& refIndex = Index.at(refID);
    const BamBinMap& binMap        = refIndex.Bins;

    // get minimum offset to consider
    const LinearOffsetVector& offsets = refIndex.Offsets;
    uint64_t minOffset = ( (unsigned int)(left>>BAM_LIDX_SHIFT) >= offsets.size() ) ? 0 : offsets.at(left>>BAM_LIDX_SHIFT);

    // store offsets to beginning of alignment 'chunks'
    std::vector<int64_t> chunkStarts;

    // store all alignment 'chunk' starts for bins in this region
    for (int i = 0; i < numBins; ++i ) {
        uint16_t binKey = bins[i];

        map<uint32_t, ChunkVector>::const_iterator binIter = binMap.find(binKey);
        if ( (binIter != binMap.end()) && ((*binIter).first == binKey) ) {

            const ChunkVector& chunks = (*binIter).second;
            std::vector<Chunk>::const_iterator chunksIter = chunks.begin();
            std::vector<Chunk>::const_iterator chunksEnd  = chunks.end();
            for ( ; chunksIter != chunksEnd; ++chunksIter) {
                const Chunk& chunk = (*chunksIter);
                if ( chunk.Stop > minOffset ) {
                    chunkStarts.push_back( chunk.Start );
                }
            }
        }
    }

    // clean up memory
    free(bins);

    // if no alignments found, else return smallest offset for alignment starts
    if ( chunkStarts.size() == 0 ) { return -1; }
    else { return *min_element(chunkStarts.begin(), chunkStarts.end()); }
}

// saves BAM bin entry for index
void BamReader::BamReaderPrivate::InsertBinEntry(BamBinMap&      binMap,
                                                 const uint32_t& saveBin,
                                                 const uint64_t& saveOffset,
                                                 const uint64_t& lastOffset)
{
    // look up saveBin
    BamBinMap::iterator binIter = binMap.find(saveBin);

    // create new chunk
    Chunk newChunk(saveOffset, lastOffset);

    // if entry doesn't exist
    if ( binIter == binMap.end() ) {
        ChunkVector newChunks;
        newChunks.push_back(newChunk);
        binMap.insert( pair<uint32_t, ChunkVector>(saveBin, newChunks));
    }

    // otherwise
    else {
        ChunkVector& binChunks = (*binIter).second;
        binChunks.push_back( newChunk );
    }
}

// saves linear offset entry for index
void BamReader::BamReaderPrivate::InsertLinearOffset(LinearOffsetVector& offsets,
                                                     const BamAlignment& bAlignment,
                                                     const uint64_t&     lastOffset)
{
    // get converted offsets
    int beginOffset = bAlignment.Position >> BAM_LIDX_SHIFT;
    int endOffset   = ( CalculateAlignmentEnd(bAlignment.Position, bAlignment.CigarData) - 1) >> BAM_LIDX_SHIFT;

    // resize vector if necessary
    int oldSize = offsets.size();
    int newSize = endOffset + 1;
    if ( oldSize < newSize ) {        
        Roundup32(newSize);
        offsets.resize(newSize, 0);
    }

    // store offset
    for(int i = beginOffset + 1; i <= endOffset ; ++i) {
        if ( offsets[i] == 0) {
            offsets[i] = lastOffset;
        }
    }
}

// returns whether alignment overlaps currently specified region (refID, leftBound)
bool BamReader::BamReaderPrivate::IsOverlap(BamAlignment& bAlignment) {

    // if on different reference sequence, quit
    if ( bAlignment.RefID != CurrentRefID ) { return false; }

    // read starts after left boundary
    if ( bAlignment.Position >= CurrentLeft) { return true; }

    // return whether alignment end overlaps left boundary
    return ( CalculateAlignmentEnd(bAlignment.Position, bAlignment.CigarData) >= CurrentLeft );
}

// jumps to specified region(refID, leftBound) in BAM file, returns success/fail
bool BamReader::BamReaderPrivate::Jump(int refID, int position) {

    // if data exists for this reference and position is valid    
    if ( References.at(refID).RefHasAlignments && (position <= References.at(refID).RefLength) ) {

		// set current region
        CurrentRefID = refID;
        CurrentLeft  = position;
        IsRegionSpecified = true;

		// calculate offset
        int64_t offset = GetOffset(CurrentRefID, CurrentLeft);

		// if in valid offset, return failure
        if ( offset == -1 ) { return false; }

		// otherwise return success of seek operation
        else { return mBGZF.Seek(offset); }
    }

	// invalid jump request parameters, return failure
    return false;
}

// load BAM header data
void BamReader::BamReaderPrivate::LoadHeaderData(void) {

    // check to see if proper BAM header
    char buffer[4];
    if (mBGZF.Read(buffer, 4) != 4) {
        printf("Could not read header type\n");
        exit(1);
    }

    if (strncmp(buffer, "BAM\001", 4)) {
        printf("wrong header type!\n");
        exit(1);
    }

    // get BAM header text length
    mBGZF.Read(buffer, 4);
    const unsigned int headerTextLength = BgzfData::UnpackUnsignedInt(buffer);

    // get BAM header text
    char* headerText = (char*)calloc(headerTextLength + 1, 1);
    mBGZF.Read(headerText, headerTextLength);
    HeaderText = (string)((const char*)headerText);

    // clean up calloc-ed temp variable
    free(headerText);
}

// load existing index data from BAM index file (".bai"), return success/fail
bool BamReader::BamReaderPrivate::LoadIndex(void) {

    // clear out index data
    ClearIndex();

    // skip if index file empty
    if ( IndexFilename.empty() ) { return false; }

    // open index file, abort on error
    FILE* indexStream = fopen(IndexFilename.c_str(), "rb");
    if(!indexStream) {
        printf("ERROR: Unable to open the BAM index file %s for reading.\n", IndexFilename.c_str() );
        return false;
    }

    // see if index is valid BAM index
    char magic[4];
    fread(magic, 1, 4, indexStream);
    if (strncmp(magic, "BAI\1", 4)) {
        printf("Problem with index file - invalid format.\n");
        fclose(indexStream);
        return false;
    }

    // get number of reference sequences
    uint32_t numRefSeqs;
    fread(&numRefSeqs, 4, 1, indexStream);

    // intialize space for BamIndex data structure
    Index.reserve(numRefSeqs);

    // iterate over reference sequences
    for (unsigned int i = 0; i < numRefSeqs; ++i) {

        // get number of bins for this reference sequence
        int32_t numBins;
        fread(&numBins, 4, 1, indexStream);

        if (numBins > 0) {
            RefData& refEntry = References[i];
            refEntry.RefHasAlignments = true;
        }

        // intialize BinVector
        BamBinMap binMap;

        // iterate over bins for that reference sequence
        for (int j = 0; j < numBins; ++j) {

            // get binID
            uint32_t binID;
            fread(&binID, 4, 1, indexStream);

            // get number of regionChunks in this bin
            uint32_t numChunks;
            fread(&numChunks, 4, 1, indexStream);

            // intialize ChunkVector
            ChunkVector regionChunks;
            regionChunks.reserve(numChunks);

            // iterate over regionChunks in this bin
            for (unsigned int k = 0; k < numChunks; ++k) {

                // get chunk boundaries (left, right)
                uint64_t left;
                uint64_t right;
                fread(&left, 8, 1, indexStream);
                fread(&right, 8, 1, indexStream);

                // save ChunkPair
                regionChunks.push_back( Chunk(left, right) );
            }

            // sort chunks for this bin
            sort( regionChunks.begin(), regionChunks.end(), ChunkLessThan );

            // save binID, chunkVector for this bin
            binMap.insert( pair<uint32_t, ChunkVector>(binID, regionChunks) );
        }

        // load linear index for this reference sequence

        // get number of linear offsets
        int32_t numLinearOffsets;
        fread(&numLinearOffsets, 4, 1, indexStream);

        // intialize LinearOffsetVector
        LinearOffsetVector offsets;
        offsets.reserve(numLinearOffsets);

        // iterate over linear offsets for this reference sequeence
        uint64_t linearOffset;
        for (int j = 0; j < numLinearOffsets; ++j) {
            // read a linear offset & store
            fread(&linearOffset, 8, 1, indexStream);
            offsets.push_back(linearOffset);
        }

        // sort linear offsets
        sort( offsets.begin(), offsets.end() );

        // store index data for that reference sequence
        Index.push_back( ReferenceIndex(binMap, offsets) );
    }

    // close index file (.bai) and return
    fclose(indexStream);
    return true;
}

// populates BamAlignment with alignment data under file pointer, returns success/fail
bool BamReader::BamReaderPrivate::LoadNextAlignment(BamAlignment& bAlignment) {

    // read in the 'block length' value, make sure it's not zero
    char buffer[4];
    mBGZF.Read(buffer, 4);
    const unsigned int blockLength = BgzfData::UnpackUnsignedInt(buffer);
    if ( blockLength == 0 ) { return false; }

    // keep track of bytes read as method progresses
    int bytesRead = 4;

    // read in core alignment data, make sure the right size of data was read
    char x[BAM_CORE_SIZE];
    if ( mBGZF.Read(x, BAM_CORE_SIZE) != BAM_CORE_SIZE ) { return false; }
    bytesRead += BAM_CORE_SIZE;

    // set BamAlignment 'core' data and character data lengths
    unsigned int tempValue;
    unsigned int queryNameLength;
    unsigned int numCigarOperations;
    unsigned int querySequenceLength;

    bAlignment.RefID    = BgzfData::UnpackSignedInt(&x[0]);
    bAlignment.Position = BgzfData::UnpackSignedInt(&x[4]);

    tempValue             = BgzfData::UnpackUnsignedInt(&x[8]);
    bAlignment.Bin        = tempValue >> 16;
    bAlignment.MapQuality = tempValue >> 8 & 0xff;
    queryNameLength       = tempValue & 0xff;

    tempValue                = BgzfData::UnpackUnsignedInt(&x[12]);
    bAlignment.AlignmentFlag = tempValue >> 16;
    numCigarOperations       = tempValue & 0xffff;

    querySequenceLength     = BgzfData::UnpackUnsignedInt(&x[16]);
    bAlignment.MateRefID    = BgzfData::UnpackSignedInt(&x[20]);
    bAlignment.MatePosition = BgzfData::UnpackSignedInt(&x[24]);
    bAlignment.InsertSize   = BgzfData::UnpackSignedInt(&x[28]);

    // calculate lengths/offsets
    const unsigned int dataLength      = blockLength - BAM_CORE_SIZE;
    const unsigned int cigarDataOffset = queryNameLength;
    const unsigned int seqDataOffset   = cigarDataOffset + (numCigarOperations * 4);
    const unsigned int qualDataOffset  = seqDataOffset + (querySequenceLength+1)/2;
    const unsigned int tagDataOffset   = qualDataOffset + querySequenceLength;
    const unsigned int tagDataLen      = dataLength - tagDataOffset;

    // set up destination buffers for character data
    char* allCharData   = (char*)calloc(sizeof(char), dataLength);
    uint32_t* cigarData = (uint32_t*)(allCharData + cigarDataOffset);
    char* seqData       = ((char*)allCharData) + seqDataOffset;
    char* qualData      = ((char*)allCharData) + qualDataOffset;
    char* tagData       = ((char*)allCharData) + tagDataOffset;

    // get character data - make sure proper data size was read
    if ( mBGZF.Read(allCharData, dataLength) != (signed int)dataLength) { return false; }
    else {

        bytesRead += dataLength;

        // clear out any previous string data
        bAlignment.Name.clear();
        bAlignment.QueryBases.clear();
        bAlignment.Qualities.clear();
        bAlignment.AlignedBases.clear();
        bAlignment.CigarData.clear();
        bAlignment.TagData.clear();

        // save name
        bAlignment.Name = (string)((const char*)(allCharData));

        // save query sequence
        for (unsigned int i = 0; i < querySequenceLength; ++i) {
            char singleBase = DNA_LOOKUP[ ( ( seqData[(i/2)] >> (4*(1-(i%2)))) & 0xf ) ];
            bAlignment.QueryBases.append( 1, singleBase );
        }

        // save sequence length
        bAlignment.Length = bAlignment.QueryBases.length();

        // save qualities, convert from numeric QV to FASTQ character
        for (unsigned int i = 0; i < querySequenceLength; ++i) {
            char singleQuality = (char)(qualData[i]+33);
            bAlignment.Qualities.append( 1, singleQuality );
        }

        // save CIGAR-related data;
        int k = 0;
        for (unsigned int i = 0; i < numCigarOperations; ++i) {

            // build CigarOp struct
            CigarOp op;
            op.Length = (cigarData[i] >> BAM_CIGAR_SHIFT);
            op.Type   = CIGAR_LOOKUP[ (cigarData[i] & BAM_CIGAR_MASK) ];

            // save CigarOp
            bAlignment.CigarData.push_back(op);

            // build AlignedBases string
            switch (op.Type) {

                case ('M') :
                case ('I') : bAlignment.AlignedBases.append( bAlignment.QueryBases.substr(k, op.Length) ); // for 'M', 'I' - write bases
                case ('S') : k += op.Length;                                                               // for 'S' - skip over query bases
                             break;

                case ('D') : bAlignment.AlignedBases.append( op.Length, '-' );	// for 'D' - write gap character
                             break;

                case ('P') : bAlignment.AlignedBases.append( op.Length, '*' );	// for 'P' - write padding character;
                             break;

                case ('N') : bAlignment.AlignedBases.append( op.Length, 'N' );  // for 'N' - write N's, skip bases in query sequence
                             k += op.Length;
                             break;

                case ('H') : break; 					        // for 'H' - do nothing, move to next op

                default    : printf("ERROR: Invalid Cigar op type\n"); // shouldn't get here
                             exit(1);
            }
        }

        // read in the tag data
        bAlignment.TagData.resize(tagDataLen);
        memcpy((char*)bAlignment.TagData.data(), tagData, tagDataLen);
    }

    free(allCharData);
    return true;
}

// loads reference data from BAM file
void BamReader::BamReaderPrivate::LoadReferenceData(void) {

    // get number of reference sequences
    char buffer[4];
    mBGZF.Read(buffer, 4);
    const unsigned int numberRefSeqs = BgzfData::UnpackUnsignedInt(buffer);
    if (numberRefSeqs == 0) { return; }
    References.reserve((int)numberRefSeqs);

    // iterate over all references in header
    for (unsigned int i = 0; i != numberRefSeqs; ++i) {

        // get length of reference name
        mBGZF.Read(buffer, 4);
        const unsigned int refNameLength = BgzfData::UnpackUnsignedInt(buffer);
        char* refName = (char*)calloc(refNameLength, 1);

        // get reference name and reference sequence length
        mBGZF.Read(refName, refNameLength);
        mBGZF.Read(buffer, 4);
        const int refLength = BgzfData::UnpackSignedInt(buffer);

        // store data for reference
        RefData aReference;
        aReference.RefName   = (string)((const char*)refName);
        aReference.RefLength = refLength;
        References.push_back(aReference);

        // clean up calloc-ed temp variable
        free(refName);
    }
}

// merges 'alignment chunks' in BAM bin (used for index building)
void BamReader::BamReaderPrivate::MergeChunks(void) {

    // iterate over reference enties
    BamIndex::iterator indexIter = Index.begin();
    BamIndex::iterator indexEnd  = Index.end();
    for ( ; indexIter != indexEnd; ++indexIter ) {

        // get BAM bin map for this reference
        ReferenceIndex& refIndex = (*indexIter);
        BamBinMap& bamBinMap = refIndex.Bins;

        // iterate over BAM bins
        BamBinMap::iterator binIter = bamBinMap.begin();
        BamBinMap::iterator binEnd  = bamBinMap.end();
        for ( ; binIter != binEnd; ++binIter ) {

            // get chunk vector for this bin
            ChunkVector& binChunks = (*binIter).second;
            if ( binChunks.size() == 0 ) { continue; }

            ChunkVector mergedChunks;
            mergedChunks.push_back( binChunks[0] );

            // iterate over chunks
            int i = 0;
            ChunkVector::iterator chunkIter = binChunks.begin();
            ChunkVector::iterator chunkEnd  = binChunks.end();
            for ( ++chunkIter; chunkIter != chunkEnd; ++chunkIter) {

                // get 'currentChunk' based on numeric index
                Chunk& currentChunk = mergedChunks[i];

                // get iteratorChunk based on vector iterator
                Chunk& iteratorChunk = (*chunkIter);

                // if currentChunk.Stop(shifted) == iterator Chunk.Start(shifted)
                if ( currentChunk.Stop>>16 == iteratorChunk.Start>>16 ) {

                    // set currentChunk.Stop to iteratorChunk.Stop
                    currentChunk.Stop = iteratorChunk.Stop;
                }

                // otherwise
                else {
                    // set currentChunk + 1 to iteratorChunk
                    mergedChunks.push_back(iteratorChunk);
                    ++i;
                }
            }

            // saved merged chunk vector
            (*binIter).second = mergedChunks;
        }
    }
}

// opens BAM file (and index)
void BamReader::BamReaderPrivate::Open(const string& filename, const string& indexFilename) {

    Filename = filename;
    IndexFilename = indexFilename;

    // open the BGZF file for reading, retrieve header text & reference data
    mBGZF.Open(filename, "rb");
    LoadHeaderData();
    LoadReferenceData();

    // store file offset of first alignment
    AlignmentsBeginOffset = mBGZF.Tell();

    // open index file & load index data (if exists)
    if ( !IndexFilename.empty() ) {
        LoadIndex();
    }
}

// returns BAM file pointer to beginning of alignment data
bool BamReader::BamReaderPrivate::Rewind(void) {

    // find first reference that has alignments in the BAM file
    int refID = 0;
    int refCount = References.size();
    for ( ; refID < refCount; ++refID ) {
        if ( References.at(refID).RefHasAlignments ) { break; }
    }

    // store default bounds for first alignment
    CurrentRefID = refID;
    CurrentLeft = 0;
    IsRegionSpecified = false;

    // return success/failure of seek
    return mBGZF.Seek(AlignmentsBeginOffset);
}

// rounds value up to next power-of-2 (used in index building)
void BamReader::BamReaderPrivate::Roundup32(int& value) {    
    --value;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    ++value;
}

// saves index data to BAM index file (".bai"), returns success/fail
bool BamReader::BamReaderPrivate::WriteIndex(void) {

    IndexFilename = Filename + ".bai";
    FILE* indexStream = fopen(IndexFilename.c_str(), "wb");
    if ( indexStream == 0 ) {
        printf("ERROR: Could not open file to save index\n");
        return false;
    }

    // write BAM index header
    fwrite("BAI\1", 1, 4, indexStream);

    // write number of reference sequences
    int32_t numReferenceSeqs = Index.size();
    fwrite(&numReferenceSeqs, 4, 1, indexStream);

    // iterate over reference sequences
    BamIndex::const_iterator indexIter = Index.begin();
    BamIndex::const_iterator indexEnd  = Index.end();
    for ( ; indexIter != indexEnd; ++ indexIter ) {

        // get reference index data
        const ReferenceIndex& refIndex = (*indexIter);
        const BamBinMap& binMap = refIndex.Bins;
        const LinearOffsetVector& offsets = refIndex.Offsets;

        // write number of bins
        int32_t binCount = binMap.size();
        fwrite(&binCount, 4, 1, indexStream);

        // iterate over bins
        BamBinMap::const_iterator binIter = binMap.begin();
        BamBinMap::const_iterator binEnd  = binMap.end();
        for ( ; binIter != binEnd; ++binIter ) {

            // get bin data (key and chunk vector)
            const uint32_t& binKey = (*binIter).first;
            const ChunkVector& binChunks = (*binIter).second;

            // save BAM bin key
            fwrite(&binKey, 4, 1, indexStream);

            // save chunk count
            int32_t chunkCount = binChunks.size();
            fwrite(&chunkCount, 4, 1, indexStream);

            // iterate over chunks
            ChunkVector::const_iterator chunkIter = binChunks.begin();
            ChunkVector::const_iterator chunkEnd  = binChunks.end();
            for ( ; chunkIter != chunkEnd; ++chunkIter ) {

                // get current chunk data
                const Chunk& chunk    = (*chunkIter);
                const uint64_t& start = chunk.Start;
                const uint64_t& stop  = chunk.Stop;

                // save chunk offsets
                fwrite(&start, 8, 1, indexStream);
                fwrite(&stop,  8, 1, indexStream);
            }
        }

        // write linear offsets size
        int32_t offsetSize = offsets.size();
        fwrite(&offsetSize, 4, 1, indexStream);

        // iterate over linear offsets
        LinearOffsetVector::const_iterator offsetIter = offsets.begin();
        LinearOffsetVector::const_iterator offsetEnd  = offsets.end();
        for ( ; offsetIter != offsetEnd; ++offsetIter ) {

            // write linear offset value
            const uint64_t& linearOffset = (*offsetIter);
            fwrite(&linearOffset, 8, 1, indexStream);
        }
    }

    // flush buffer, close file, and return success
    fflush(indexStream);
    fclose(indexStream);
    return true;
}
                                                                                                                                                                                                                                                                    BamReader.h                                                                                         0000664 0001074 0001074 00000005312 11307520655 012736  0                                                                                                    ustar   barnett                         barnett                                                                                                                                                                                                                // ***************************************************************************
// BamReader.h (c) 2009 Derek Barnett, Michael Str�mberg
// Marth Lab, Department of Biology, Boston College
// All rights reserved.
// ---------------------------------------------------------------------------
// Last modified: 8 December 2009 (DB)
// ---------------------------------------------------------------------------
// Uses BGZF routines were adapted from the bgzf.c code developed at the Broad
// Institute.
// ---------------------------------------------------------------------------
// Provides the basic functionality for reading BAM files
// ***************************************************************************

#ifndef BAMREADER_H
#define BAMREADER_H

// C++ includes
#include <string>

// BamTools includes
#include "BamAux.h"

namespace BamTools {

class BamReader {

    // constructor / destructor
    public:
        BamReader(void);
        ~BamReader(void);

    // public interface
    public:

        // ----------------------
        // BAM file operations
        // ----------------------

        // close BAM file
        void Close(void);
        // performs random-access jump to reference, position
        bool Jump(int refID, int position = 0);
        // opens BAM file (and optional BAM index file, if provided)
        void Open(const std::string& filename, const std::string& indexFilename = "");
        // returns file pointer to beginning of alignments
        bool Rewind(void);

        // ----------------------
        // access alignment data
        // ----------------------

        // retrieves next available alignment (returns success/fail)
        bool GetNextAlignment(BamAlignment& bAlignment);

        // ----------------------
        // access auxiliary data
        // ----------------------

        // returns SAM header text
        const std::string GetHeaderText(void) const;
        // returns number of reference sequences
        const int GetReferenceCount(void) const;
        // returns vector of reference objects
        const BamTools::RefVector GetReferenceData(void) const;
        // returns reference id (used for BamReader::Jump()) for the given reference name
        const int GetReferenceID(const std::string& refName) const;

        // ----------------------
        // BAM index operations
        // ----------------------

        // creates index for BAM file, saves to file (default = bamFilename + ".bai")
        bool CreateIndex(void);

    // private implementation
    private:
        struct BamReaderPrivate;
        BamReaderPrivate* d;
};

} // namespace BamTools

#endif // BAMREADER_H
                                                                                                                                                                                                                                                                                                                      BamTrimMain.cpp                                                                                     0000664 0001074 0001074 00000005351 11307520655 013612  0                                                                                                    ustar   barnett                         barnett                                                                                                                                                                                                                // ***************************************************************************
// BamTrimMain.cpp (c) 2009 Derek Barnett
// Marth Lab, Department of Biology, Boston College
// All rights reserved.
// ---------------------------------------------------------------------------
// Last modified: 15 July 2009 (DB)
// ---------------------------------------------------------------------------
// Basic example of reading/writing BAM files. Pulls alignments overlapping 
// the range specified by user from one BAM file and writes to a new BAM file.
// ***************************************************************************

// Std C/C++ includes
#include <cstdlib>
#include <iostream>
#include <string>
using namespace std;

// BamTools includes
#include "BamReader.h"
#include "BamWriter.h"
using namespace BamTools;

int main(int argc, char* argv[]) {

	// validate argument count
	if( argc != 7 ) {
		cerr << "USAGE: " << argv[0] << " <input BAM file> <input BAM index file> <output BAM file> <reference name> <leftBound> <rightBound> " << endl;
		exit(1);
	}

	// store arguments
	string inBamFilename  = argv[1];
	string indexFilename  = argv[2];
	string outBamFilename = argv[3];
	string referenceName  = argv[4];
	string leftBound_str  = argv[5];
	string rightBound_str = argv[6];

	// open our BAM reader
	BamReader reader;
	reader.Open(inBamFilename, indexFilename);

	// get header & reference information
	string header = reader.GetHeaderText();
	RefVector references = reader.GetReferenceData();
	
	// open our BAM writer
	BamWriter writer;
	writer.Open(outBamFilename, header, references);

	// get reference ID from name
	int refID = 0;
	RefVector::const_iterator refIter = references.begin();
	RefVector::const_iterator refEnd  = references.end();
	for ( ; refIter != refEnd; ++refIter ) {
		if ( (*refIter).RefName == referenceName ) { break; }
		++refID;
	}
	
	// validate ID
	if ( refIter == refEnd ) {
		cerr << "Reference: " << referenceName << " not found." << endl;
		exit(1);
	}

	// convert boundary arguments to numeric values
	int leftBound  = (int) atoi( leftBound_str.c_str()  );
	int rightBound = (int) atoi( rightBound_str.c_str() );
	
	// attempt jump to range of interest
	if ( reader.Jump(refID, leftBound) ) {
		
		// while data exists and alignment begin before right bound
		BamAlignment bAlignment;
		while ( reader.GetNextAlignment(bAlignment) && (bAlignment.Position <= rightBound) ) {
			// save alignment to archive
			writer.SaveAlignment(bAlignment);
		}
	} 
	
	// if jump failed
	else {
		cerr << "Could not jump to ref:pos " << referenceName << ":" << leftBound << endl;
		exit(1);
	}

	// clean up and exit
	reader.Close();
	writer.Close();	
	return 0;
}                                                                                                                                                                                                                                                                                       BamWriter.cpp                                                                                       0000664 0001074 0001074 00000022463 11307516613 013350  0                                                                                                    ustar   barnett                         barnett                                                                                                                                                                                                                // ***************************************************************************
// BamWriter.cpp (c) 2009 Michael Str�mberg, Derek Barnett
// Marth Lab, Department of Biology, Boston College
// All rights reserved.
// ---------------------------------------------------------------------------
// Last modified: 8 December 2009 (DB)
// ---------------------------------------------------------------------------
// Uses BGZF routines were adapted from the bgzf.c code developed at the Broad
// Institute.
// ---------------------------------------------------------------------------
// Provides the basic functionality for producing BAM files
// ***************************************************************************

// BGZF includes
#include "BGZF.h"
#include "BamWriter.h"
using namespace BamTools;
using namespace std;

struct BamWriter::BamWriterPrivate {

    // data members
    BgzfData mBGZF;

    // constructor / destructor
    BamWriterPrivate(void) { }
    ~BamWriterPrivate(void) {
        mBGZF.Close();
    }

    // "public" interface
    void Close(void);
    void Open(const std::string& filename, const std::string& samHeader, const BamTools::RefVector& referenceSequences);
    void SaveAlignment(const BamTools::BamAlignment& al);

    // internal methods
    void CreatePackedCigar(const std::vector<CigarOp>& cigarOperations, std::string& packedCigar);
    void EncodeQuerySequence(const std::string& query, std::string& encodedQuery);
};

// -----------------------------------------------------
// BamWriter implementation
// -----------------------------------------------------

// constructor
BamWriter::BamWriter(void) {
    d = new BamWriterPrivate;
}

// destructor
BamWriter::~BamWriter(void) {
    delete d;
    d = 0;
}

// closes the alignment archive
void BamWriter::Close(void) {
    d->Close();
}

// opens the alignment archive
void BamWriter::Open(const string& filename, const string& samHeader, const RefVector& referenceSequences) {
    d->Open(filename, samHeader, referenceSequences);
}

// saves the alignment to the alignment archive
void BamWriter::SaveAlignment(const BamAlignment& al) {
    d->SaveAlignment(al);
}

// -----------------------------------------------------
// BamWriterPrivate implementation
// -----------------------------------------------------

// closes the alignment archive
void BamWriter::BamWriterPrivate::Close(void) {
    mBGZF.Close();
}

// creates a cigar string from the supplied alignment
void BamWriter::BamWriterPrivate::CreatePackedCigar(const vector<CigarOp>& cigarOperations, string& packedCigar) {

    // initialize
    const unsigned int numCigarOperations = cigarOperations.size();
    packedCigar.resize(numCigarOperations * BT_SIZEOF_INT);

    // pack the cigar data into the string
    unsigned int* pPackedCigar = (unsigned int*)packedCigar.data();

    unsigned int cigarOp;
    vector<CigarOp>::const_iterator coIter;
    for(coIter = cigarOperations.begin(); coIter != cigarOperations.end(); coIter++) {

        switch(coIter->Type) {
            case 'M':
                  cigarOp = BAM_CMATCH;
                  break;
            case 'I':
                  cigarOp = BAM_CINS;
                  break;
            case 'D':
                  cigarOp = BAM_CDEL;
                  break;
            case 'N':
                  cigarOp = BAM_CREF_SKIP;
                  break;
            case 'S':
                  cigarOp = BAM_CSOFT_CLIP;
                  break;
            case 'H':
                  cigarOp = BAM_CHARD_CLIP;
                  break;
            case 'P':
                  cigarOp = BAM_CPAD;
                  break;
            default:
                  printf("ERROR: Unknown cigar operation found: %c\n", coIter->Type);
                  exit(1);
        }

        *pPackedCigar = coIter->Length << BAM_CIGAR_SHIFT | cigarOp;
        pPackedCigar++;
    }
}

// encodes the supplied query sequence into 4-bit notation
void BamWriter::BamWriterPrivate::EncodeQuerySequence(const string& query, string& encodedQuery) {

    // prepare the encoded query string
    const unsigned int queryLen = query.size();
    const unsigned int encodedQueryLen = (unsigned int)((queryLen / 2.0) + 0.5);
    encodedQuery.resize(encodedQueryLen);
    char* pEncodedQuery = (char*)encodedQuery.data();
    const char* pQuery = (const char*)query.data();

    unsigned char nucleotideCode;
    bool useHighWord = true;

    while(*pQuery) {

        switch(*pQuery) {
            case '=':
                    nucleotideCode = 0;
                    break;
            case 'A':
                    nucleotideCode = 1;
                    break;
            case 'C':
                    nucleotideCode = 2;
                    break;
            case 'G':
                    nucleotideCode = 4;
                    break;
            case 'T':
                    nucleotideCode = 8;
                    break;
            case 'N':
                    nucleotideCode = 15;
                    break;
            default:
                    printf("ERROR: Only the following bases are supported in the BAM format: {=, A, C, G, T, N}. Found [%c]\n", *pQuery);
                    exit(1);
        }

        // pack the nucleotide code
        if(useHighWord) {
            *pEncodedQuery = nucleotideCode << 4;
            useHighWord = false;
        } else {
            *pEncodedQuery |= nucleotideCode;
            pEncodedQuery++;
            useHighWord = true;
        }

        // increment the query position
        pQuery++;
    }
}

// opens the alignment archive
void BamWriter::BamWriterPrivate::Open(const string& filename, const string& samHeader, const RefVector& referenceSequences) {

    // open the BGZF file for writing
    mBGZF.Open(filename, "wb");

    // ================
    // write the header
    // ================

    // write the BAM signature
    const unsigned char SIGNATURE_LENGTH = 4;
    const char* BAM_SIGNATURE = "BAM\1";
    mBGZF.Write(BAM_SIGNATURE, SIGNATURE_LENGTH);

    // write the SAM header text length
    const unsigned int samHeaderLen = samHeader.size();
    mBGZF.Write((char*)&samHeaderLen, BT_SIZEOF_INT);

    // write the SAM header text
    if(samHeaderLen > 0) {
        mBGZF.Write(samHeader.data(), samHeaderLen);
    }

    // write the number of reference sequences
    const unsigned int numReferenceSequences = referenceSequences.size();
    mBGZF.Write((char*)&numReferenceSequences, BT_SIZEOF_INT);

    // =============================
    // write the sequence dictionary
    // =============================

    RefVector::const_iterator rsIter;
    for(rsIter = referenceSequences.begin(); rsIter != referenceSequences.end(); rsIter++) {

        // write the reference sequence name length
        const unsigned int referenceSequenceNameLen = rsIter->RefName.size() + 1;
        mBGZF.Write((char*)&referenceSequenceNameLen, BT_SIZEOF_INT);

        // write the reference sequence name
        mBGZF.Write(rsIter->RefName.c_str(), referenceSequenceNameLen);

        // write the reference sequence length
        mBGZF.Write((char*)&rsIter->RefLength, BT_SIZEOF_INT);
    }
}

// saves the alignment to the alignment archive
void BamWriter::BamWriterPrivate::SaveAlignment(const BamAlignment& al) {

    // initialize
    const unsigned int nameLen            = al.Name.size() + 1;
    const unsigned int queryLen           = al.QueryBases.size();
    const unsigned int numCigarOperations = al.CigarData.size();

    // create our packed cigar string
    string packedCigar;
    CreatePackedCigar(al.CigarData, packedCigar);
    const unsigned int packedCigarLen = packedCigar.size();

    // encode the query
    string encodedQuery;
    EncodeQuerySequence(al.QueryBases, encodedQuery);
    const unsigned int encodedQueryLen = encodedQuery.size();

    // store the tag data length
    const unsigned int tagDataLength = al.TagData.size() + 1;

    // assign the BAM core data
    unsigned int buffer[8];
    buffer[0] = al.RefID;
    buffer[1] = al.Position;
    buffer[2] = (al.Bin << 16) | (al.MapQuality << 8) | nameLen;
    buffer[3] = (al.AlignmentFlag << 16) | numCigarOperations;
    buffer[4] = queryLen;
    buffer[5] = al.MateRefID;
    buffer[6] = al.MatePosition;
    buffer[7] = al.InsertSize;

    // write the block size
    const unsigned int dataBlockSize = nameLen + packedCigarLen + encodedQueryLen + queryLen + tagDataLength;
    const unsigned int blockSize = BAM_CORE_SIZE + dataBlockSize;
    mBGZF.Write((char*)&blockSize, BT_SIZEOF_INT);

    // write the BAM core
    mBGZF.Write((char*)&buffer, BAM_CORE_SIZE);

    // write the query name
    mBGZF.Write(al.Name.c_str(), nameLen);

    // write the packed cigar
    mBGZF.Write(packedCigar.data(), packedCigarLen);

    // write the encoded query sequence
    mBGZF.Write(encodedQuery.data(), encodedQueryLen);

    // write the base qualities
    string baseQualities = al.Qualities;
    char* pBaseQualities = (char*)al.Qualities.data();
    for(unsigned int i = 0; i < queryLen; i++) { pBaseQualities[i] -= 33; }
    mBGZF.Write(pBaseQualities, queryLen);

    // write the read group tag
    mBGZF.Write(al.TagData.data(), tagDataLength);
}
                                                                                                                                                                                                             BamWriter.h                                                                                         0000664 0001074 0001074 00000003034 11307516613 013006  0                                                                                                    ustar   barnett                         barnett                                                                                                                                                                                                                // ***************************************************************************
// BamWriter.h (c) 2009 Michael Str�mberg, Derek Barnett
// Marth Lab, Department of Biology, Boston College
// All rights reserved.
// ---------------------------------------------------------------------------
// Last modified: 8 December 2009 (DB)
// ---------------------------------------------------------------------------
// Uses BGZF routines were adapted from the bgzf.c code developed at the Broad
// Institute.
// ---------------------------------------------------------------------------
// Provides the basic functionality for producing BAM files
// ***************************************************************************

#ifndef BAMWRITER_H
#define BAMWRITER_H

// C++ includes
#include <string>

// BamTools includes
#include "BamAux.h"

namespace BamTools {

class BamWriter {

    // constructor/destructor
    public:
        BamWriter(void);
        ~BamWriter(void);

    // public interface
    public:
        // closes the alignment archive
        void Close(void);
        // opens the alignment archive
        void Open(const std::string& filename, const std::string& samHeader, const BamTools::RefVector& referenceSequences);
        // saves the alignment to the alignment archive
        void SaveAlignment(const BamTools::BamAlignment& al);

    // private implementation
    private:
        struct BamWriterPrivate;
        BamWriterPrivate* d;
};

} // namespace BamTools

#endif // BAMWRITER_H
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    BGZF.cpp                                                                                            0000664 0001074 0001074 00000024215 11307516613 012201  0                                                                                                    ustar   barnett                         barnett                                                                                                                                                                                                                // ***************************************************************************
// BGZF.cpp (c) 2009 Derek Barnett, Michael Str�mberg
// Marth Lab, Department of Biology, Boston College
// All rights reserved.
// ---------------------------------------------------------------------------
// Last modified: 8 December 2009 (DB)
// ---------------------------------------------------------------------------
// BGZF routines were adapted from the bgzf.c code developed at the Broad
// Institute.
// ---------------------------------------------------------------------------
// Provides the basic functionality for reading & writing BGZF files
// ***************************************************************************

#include <algorithm>
#include "BGZF.h"
using namespace BamTools;
using std::string;
using std::min;

BgzfData::BgzfData(void)
    : UncompressedBlockSize(DEFAULT_BLOCK_SIZE)
    , CompressedBlockSize(MAX_BLOCK_SIZE)
    , BlockLength(0)
    , BlockOffset(0)
    , BlockAddress(0)
    , IsOpen(false)
    , IsWriteOnly(false)
    , Stream(NULL)
    , UncompressedBlock(NULL)
    , CompressedBlock(NULL)
{
    try {
        CompressedBlock   = new char[CompressedBlockSize];
        UncompressedBlock = new char[UncompressedBlockSize];
    } catch( std::bad_alloc& ba ) {
        printf("ERROR: Unable to allocate memory for our BGZF object.\n");
        exit(1);
    }
}

// destructor
BgzfData::~BgzfData(void) {
    if(CompressedBlock)   delete [] CompressedBlock;
    if(UncompressedBlock) delete [] UncompressedBlock;
}

// closes BGZF file
void BgzfData::Close(void) {

    if (!IsOpen) { return; }
    IsOpen = false;

    // flush the BGZF block
    if ( IsWriteOnly ) { FlushBlock(); }

    // flush and close
    fflush(Stream);
    fclose(Stream);
}

// compresses the current block
int BgzfData::DeflateBlock(void) {

    // initialize the gzip header
    char* buffer = CompressedBlock;
    unsigned int bufferSize = CompressedBlockSize;

    memset(buffer, 0, 18);
    buffer[0]  = GZIP_ID1;
    buffer[1]  = (char)GZIP_ID2;
    buffer[2]  = CM_DEFLATE;
    buffer[3]  = FLG_FEXTRA;
    buffer[9]  = (char)OS_UNKNOWN;
    buffer[10] = BGZF_XLEN;
    buffer[12] = BGZF_ID1;
    buffer[13] = BGZF_ID2;
    buffer[14] = BGZF_LEN;

    // loop to retry for blocks that do not compress enough
    int inputLength = BlockOffset;
    int compressedLength = 0;

    while(true) {

        z_stream zs;
        zs.zalloc    = NULL;
        zs.zfree     = NULL;
        zs.next_in   = (Bytef*)UncompressedBlock;
        zs.avail_in  = inputLength;
        zs.next_out  = (Bytef*)&buffer[BLOCK_HEADER_LENGTH];
        zs.avail_out = bufferSize - BLOCK_HEADER_LENGTH - BLOCK_FOOTER_LENGTH;

        // initialize the zlib compression algorithm
        if(deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, GZIP_WINDOW_BITS, Z_DEFAULT_MEM_LEVEL, Z_DEFAULT_STRATEGY) != Z_OK) {
            printf("ERROR: zlib deflate initialization failed.\n");
            exit(1);
        }

        // compress the data
        int status = deflate(&zs, Z_FINISH);
        if(status != Z_STREAM_END) {

            deflateEnd(&zs);

            // reduce the input length and try again
            if(status == Z_OK) {
                inputLength -= 1024;
                if(inputLength < 0) {
                    printf("ERROR: input reduction failed.\n");
                    exit(1);
                }
                continue;
            }

            printf("ERROR: zlib deflate failed.\n");
            exit(1);
        }

        // finalize the compression routine
        if(deflateEnd(&zs) != Z_OK) {
            printf("ERROR: deflate end failed.\n");
            exit(1);
        }

        compressedLength = zs.total_out;
        compressedLength += BLOCK_HEADER_LENGTH + BLOCK_FOOTER_LENGTH;

        if(compressedLength > MAX_BLOCK_SIZE) {
            printf("ERROR: deflate overflow.\n");
            exit(1);
        }

        break;
    }

    // store the compressed length
    BgzfData::PackUnsignedShort(&buffer[16], (unsigned short)(compressedLength - 1));

    // store the CRC32 checksum
    unsigned int crc = crc32(0, NULL, 0);
    crc = crc32(crc, (Bytef*)UncompressedBlock, inputLength);
    BgzfData::PackUnsignedInt(&buffer[compressedLength - 8], crc);
    BgzfData::PackUnsignedInt(&buffer[compressedLength - 4], inputLength);

    // ensure that we have less than a block of data left
    int remaining = BlockOffset - inputLength;
    if(remaining > 0) {
        if(remaining > inputLength) {
            printf("ERROR: remainder too large.\n");
            exit(1);
        }
        memcpy(UncompressedBlock, UncompressedBlock + inputLength, remaining);
    }

    BlockOffset = remaining;
    return compressedLength;
}

// flushes the data in the BGZF block
void BgzfData::FlushBlock(void) {

    // flush all of the remaining blocks
    while(BlockOffset > 0) {

        // compress the data block
        int blockLength = DeflateBlock();

        // flush the data to our output stream
        int numBytesWritten = fwrite(CompressedBlock, 1, blockLength, Stream);

        if(numBytesWritten != blockLength) {
            printf("ERROR: Expected to write %u bytes during flushing, but wrote %u bytes.\n", blockLength, numBytesWritten);
            exit(1);
        }

        BlockAddress += blockLength;
    }
}

// de-compresses the current block
int BgzfData::InflateBlock(const int& blockLength) {

    // Inflate the block in m_BGZF.CompressedBlock into m_BGZF.UncompressedBlock
    z_stream zs;
    zs.zalloc    = NULL;
    zs.zfree     = NULL;
    zs.next_in   = (Bytef*)CompressedBlock + 18;
    zs.avail_in  = blockLength - 16;
    zs.next_out  = (Bytef*)UncompressedBlock;
    zs.avail_out = UncompressedBlockSize;

    int status = inflateInit2(&zs, GZIP_WINDOW_BITS);
    if (status != Z_OK) {
        printf("inflateInit failed\n");
        exit(1);
    }

    status = inflate(&zs, Z_FINISH);
    if (status != Z_STREAM_END) {
        inflateEnd(&zs);
        printf("inflate failed\n");
        exit(1);
    }

    status = inflateEnd(&zs);
    if (status != Z_OK) {
        printf("inflateEnd failed\n");
        exit(1);
    }

    return zs.total_out;
}

void BgzfData::Open(const string& filename, const char* mode) {

    if ( strcmp(mode, "rb") == 0 ) {
        IsWriteOnly = false;
    } else if ( strcmp(mode, "wb") == 0) {
        IsWriteOnly = true;
    } else {
        printf("ERROR: Unknown file mode: %s\n", mode);
        exit(1);
    }

    Stream = fopen(filename.c_str(), mode);
    if(!Stream) {
        printf("ERROR: Unable to open the BAM file %s\n", filename.c_str() );
        exit(1);
    }
    IsOpen = true;
}

int BgzfData::Read(char* data, const unsigned int dataLength) {

   if (dataLength == 0) { return 0; }

   char* output = data;
   unsigned int numBytesRead = 0;
   while (numBytesRead < dataLength) {

       int bytesAvailable = BlockLength - BlockOffset;
       if (bytesAvailable <= 0) {
           if ( ReadBlock() != 0 ) { return -1; }
           bytesAvailable = BlockLength - BlockOffset;
           if ( bytesAvailable <= 0 ) { break; }
       }

       char* buffer   = UncompressedBlock;
       int copyLength = min( (int)(dataLength-numBytesRead), bytesAvailable );
       memcpy(output, buffer + BlockOffset, copyLength);

       BlockOffset  += copyLength;
       output       += copyLength;
       numBytesRead += copyLength;
   }

   if ( BlockOffset == BlockLength ) {
       BlockAddress = ftell(Stream);
       BlockOffset  = 0;
       BlockLength  = 0;
   }

   return numBytesRead;
}

int BgzfData::ReadBlock(void) {

    char    header[BLOCK_HEADER_LENGTH];
    int64_t blockAddress = ftell(Stream);

    int count = fread(header, 1, sizeof(header), Stream);
    if (count == 0) {
        BlockLength = 0;
        return 0;
    }

    if (count != sizeof(header)) {
        printf("read block failed - count != sizeof(header)\n");
        return -1;
    }

    if (!BgzfData::CheckBlockHeader(header)) {
        printf("read block failed - CheckBlockHeader() returned false\n");
        return -1;
    }

    int blockLength = BgzfData::UnpackUnsignedShort(&header[16]) + 1;
    char* compressedBlock = CompressedBlock;
    memcpy(compressedBlock, header, BLOCK_HEADER_LENGTH);
    int remaining = blockLength - BLOCK_HEADER_LENGTH;

    count = fread(&compressedBlock[BLOCK_HEADER_LENGTH], 1, remaining, Stream);
    if (count != remaining) {
        printf("read block failed - count != remaining\n");
        return -1;
    }

    count = InflateBlock(blockLength);
    if (count < 0) { return -1; }

    if ( BlockLength != 0 ) {
        BlockOffset = 0;
    }

    BlockAddress = blockAddress;
    BlockLength  = count;
    return 0;
}

bool BgzfData::Seek(int64_t position) {

    int     blockOffset  = (position & 0xFFFF);
    int64_t blockAddress = (position >> 16) & 0xFFFFFFFFFFFFLL;

    if (fseek(Stream, blockAddress, SEEK_SET) != 0) {
        printf("ERROR: Unable to seek in BAM file\n");
        exit(1);
    }

    BlockLength  = 0;
    BlockAddress = blockAddress;
    BlockOffset  = blockOffset;
    return true;
}

int64_t BgzfData::Tell(void) {
    return ( (BlockAddress << 16) | (BlockOffset & 0xFFFF) );
}

// writes the supplied data into the BGZF buffer
unsigned int BgzfData::Write(const char* data, const unsigned int dataLen) {

    // initialize
    unsigned int numBytesWritten = 0;
    const char* input = data;
    unsigned int blockLength = UncompressedBlockSize;

    // copy the data to the buffer
    while(numBytesWritten < dataLen) {
        unsigned int copyLength = min(blockLength - BlockOffset, dataLen - numBytesWritten);
        char* buffer = UncompressedBlock;
        memcpy(buffer + BlockOffset, input, copyLength);

        BlockOffset     += copyLength;
        input           += copyLength;
        numBytesWritten += copyLength;

        if(BlockOffset == blockLength) {
            FlushBlock();
        }
    }

    return numBytesWritten;
}
                                                                                                                                                                                                                                                                                                                                                                                   BGZF.h                                                                                              0000664 0001074 0001074 00000013625 11307516613 011651  0                                                                                                    ustar   barnett                         barnett                                                                                                                                                                                                                // ***************************************************************************
// BGZF.h (c) 2009 Derek Barnett, Michael Str�mberg
// Marth Lab, Department of Biology, Boston College
// All rights reserved.
// ---------------------------------------------------------------------------
// Last modified: 8 December 2009 (DB)
// ---------------------------------------------------------------------------
// BGZF routines were adapted from the bgzf.c code developed at the Broad
// Institute.
// ---------------------------------------------------------------------------
// Provides the basic functionality for reading & writing BGZF files
// ***************************************************************************

#ifndef BGZF_H
#define BGZF_H

// 'C' includes
#include <cstdio>
#include <cstdlib>
#include <cstring>

// C++ includes
#include <string>

// zlib includes
#include "zlib.h"

// Platform-specific type definitions
#ifdef _MSC_VER
        typedef char                 int8_t;
        typedef unsigned char       uint8_t;
        typedef short               int16_t;
        typedef unsigned short     uint16_t;
        typedef int                 int32_t;
        typedef unsigned int       uint32_t;
        typedef long long           int64_t;
        typedef unsigned long long uint64_t;
#else
        #include <stdint.h>
#endif

namespace BamTools {

// zlib constants
const int GZIP_ID1   = 31;
const int GZIP_ID2   = 139;
const int CM_DEFLATE = 8;
const int FLG_FEXTRA = 4;
const int OS_UNKNOWN = 255;
const int BGZF_XLEN  = 6;
const int BGZF_ID1   = 66;
const int BGZF_ID2   = 67;
const int BGZF_LEN   = 2;
const int GZIP_WINDOW_BITS    = -15;
const int Z_DEFAULT_MEM_LEVEL = 8;

// BZGF constants
const int BLOCK_HEADER_LENGTH = 18;
const int BLOCK_FOOTER_LENGTH = 8;
const int MAX_BLOCK_SIZE      = 65536;
const int DEFAULT_BLOCK_SIZE  = 65536;

struct BgzfData {

    // data members
    unsigned int UncompressedBlockSize;
    unsigned int CompressedBlockSize;
    unsigned int BlockLength;
    unsigned int BlockOffset;
    uint64_t BlockAddress;
    bool     IsOpen;
    bool     IsWriteOnly;
    FILE*    Stream;
    char*    UncompressedBlock;
    char*    CompressedBlock;

    // constructor & destructor
    BgzfData(void);
    ~BgzfData(void);

    // closes BGZF file
    void Close(void);
    // compresses the current block
    int DeflateBlock(void);
    // flushes the data in the BGZF block
    void FlushBlock(void);
    // de-compresses the current block
    int InflateBlock(const int& blockLength);
    // opens the BGZF file for reading (mode is either "rb" for reading, or "wb" for writing
    void Open(const std::string& filename, const char* mode);
    // reads BGZF data into a byte buffer
    int Read(char* data, const unsigned int dataLength);
    // reads BGZF block
    int ReadBlock(void);
    // seek to position in BAM file
    bool Seek(int64_t position);
    // get file position in BAM file
    int64_t Tell(void);
    // writes the supplied data into the BGZF buffer
    unsigned int Write(const char* data, const unsigned int dataLen);

    // checks BGZF block header
    static inline bool CheckBlockHeader(char* header);
    // packs an unsigned integer into the specified buffer
    static inline void PackUnsignedInt(char* buffer, unsigned int value);
    // packs an unsigned short into the specified buffer
    static inline void PackUnsignedShort(char* buffer, unsigned short value);
    // unpacks a buffer into a signed int
    static inline signed int UnpackSignedInt(char* buffer);
    // unpacks a buffer into a unsigned int
    static inline unsigned int UnpackUnsignedInt(char* buffer);
    // unpacks a buffer into a unsigned short
    static inline unsigned short UnpackUnsignedShort(char* buffer);
};

// -------------------------------------------------------------

inline
bool BgzfData::CheckBlockHeader(char* header) {

    return (header[0] == GZIP_ID1 &&
            header[1] == (char)GZIP_ID2 &&
            header[2] == Z_DEFLATED &&
            (header[3] & FLG_FEXTRA) != 0 &&
            BgzfData::UnpackUnsignedShort(&header[10]) == BGZF_XLEN &&
            header[12] == BGZF_ID1 &&
            header[13] == BGZF_ID2 &&
            BgzfData::UnpackUnsignedShort(&header[14]) == BGZF_LEN );
}

// packs an unsigned integer into the specified buffer
inline
void BgzfData::PackUnsignedInt(char* buffer, unsigned int value) {
    buffer[0] = (char)value;
    buffer[1] = (char)(value >> 8);
    buffer[2] = (char)(value >> 16);
    buffer[3] = (char)(value >> 24);
}

// packs an unsigned short into the specified buffer
inline
void BgzfData::PackUnsignedShort(char* buffer, unsigned short value) {
    buffer[0] = (char)value;
    buffer[1] = (char)(value >> 8);
}

// unpacks a buffer into a signed int
inline
signed int BgzfData::UnpackSignedInt(char* buffer) {
    union { signed int value; unsigned char valueBuffer[sizeof(signed int)]; } un;
    un.value = 0;
    un.valueBuffer[0] = buffer[0];
    un.valueBuffer[1] = buffer[1];
    un.valueBuffer[2] = buffer[2];
    un.valueBuffer[3] = buffer[3];
    return un.value;
}

// unpacks a buffer into an unsigned int
inline
unsigned int BgzfData::UnpackUnsignedInt(char* buffer) {
    union { unsigned int value; unsigned char valueBuffer[sizeof(unsigned int)]; } un;
    un.value = 0;
    un.valueBuffer[0] = buffer[0];
    un.valueBuffer[1] = buffer[1];
    un.valueBuffer[2] = buffer[2];
    un.valueBuffer[3] = buffer[3];
    return un.value;
}

// unpacks a buffer into an unsigned short
inline
unsigned short BgzfData::UnpackUnsignedShort(char* buffer) {
    union { unsigned short value; unsigned char valueBuffer[sizeof(unsigned short)];} un;
    un.value = 0;
    un.valueBuffer[0] = buffer[0];
    un.valueBuffer[1] = buffer[1];
    return un.value;
}

} // namespace BamTools

#endif // BGZF_H
                                                                                                           Makefile                                                                                            0000664 0001074 0001074 00000001044 11307517214 012376  0                                                                                                    ustar   barnett                         barnett                                                                                                                                                                                                                CXX=		g++
CXXFLAGS=	-Wall -O3
PROG=		BamConversion BamDump BamTrim
LIBS=		-lz

all: $(PROG)

BamConversion: BGZF.o BamReader.o BamWriter.o BamConversionMain.o
	$(CXX) $(CXXFLAGS) -o $@ BGZF.o BamReader.o BamWriter.o BamConversionMain.o $(LIBS)

BamDump: BGZF.o BamReader.o BamDumpMain.o
	$(CXX) $(CXXFLAGS) -o $@ BGZF.o BamReader.o BamDumpMain.o $(LIBS)

BamTrim: BGZF.o BamReader.o BamWriter.o BamTrimMain.o
	$(CXX) $(CXXFLAGS) -o $@ BGZF.o BamReader.o BamWriter.o BamTrimMain.o $(LIBS)

clean:
	rm -fr gmon.out *.o *.a a.out *~
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            README                                                                                              0000664 0001074 0001074 00000003607 11307522302 011617  0                                                                                                    ustar   barnett                         barnett                                                                                                                                                                                                                ------------------------------------------------------------
README : BAMTOOLS
------------------------------------------------------------

BamTools: a C++ API for reading/writing BAM files.

I.   Introduction
II.  Usage
III. Contact

------------------------------------------------------------

I. Introduction:

The API consists of 2 main modules - BamReader and BamWriter. As you would expect, 
BamReader provides read-access to BAM files, while BamWriter does the writing of BAM
files. BamReader provides an interface for random-access (jumping) in a BAM file,
as well as generating BAM index files.
 
An additional file, BamAux.h, is included as well.  
This file contains the common data structures and typedefs used throught the API.

BGZF.h & BGZF.cpp contain our implementation of the Broad Institute's 
BGZF compression format.

BamConversion, BamDump, and BamTrim are 3 'toy' examples on how the API can be used.

------------------------------------------------------------

II. Usage : 

To use this API, you simply need to do 3 things:

    1 - Drop the BamTools files somewhere the compiler can find them.
        (i.e. in your source tree, or somewhere else in your include path)

    2 - Import BamTools API with the following lines of code
        #include "BamReader.h"     // as needed
        #include "BamWriter.h"     // as needed
        using namespace BamTools;
    	
    3 - Compile with '-lz' ('l' as in Lima) to access ZLIB compression library
	    (For VS users, I can provide you zlib headers - just contact me).

See any included programs and Makefile for more specific compiling/usage examples.
See comments in the header files for more detailed API documentation. 

------------------------------------------------------------

III. Contact :

Feel free to contact me with any questions, comments, suggestions, bug reports, etc.
  - Derek Barnett
  
http://sourceforge.net/projects/bamtools
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         
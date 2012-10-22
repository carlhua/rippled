#ifndef __RIPPLE_CALC__
#define __RIPPLE_CALC__

#include <boost/unordered_set.hpp>

#include "LedgerEntrySet.h"

class PaymentNode {
protected:
	friend class RippleCalc;
	friend class PathState;

	uint16							uFlags;				// --> From path.

	uint160							uAccountID;			// --> Accounts: Recieving/sending account.
	uint160							uCurrencyID;		// --> Accounts: Receive and send, Offers: send.
														// --- For offer's next has currency out.
	uint160							uIssuerID;			// --> Currency's issuer

	STAmount						saTransferRate;		// Transfer rate for uIssuerID.

	// Computed by Reverse.
	STAmount						saRevRedeem;		// <-- Amount to redeem to next.
	STAmount						saRevIssue;			// <-- Amount to issue to next limited by credit and outstanding IOUs.
														//     Issue isn't used by offers.
	STAmount						saRevDeliver;		// <-- Amount to deliver to next regardless of fee.

	// Computed by forward.
	STAmount						saFwdRedeem;		// <-- Amount node will redeem to next.
	STAmount						saFwdIssue;			// <-- Amount node will issue to next.
														//	   Issue isn't used by offers.
	STAmount						saFwdDeliver;		// <-- Amount to deliver to next regardless of fee.

	// For offers:

	STAmount						saRateMax;			// XXX Should rate be sticky for forward too?

	// Directory
	uint256							uDirectTip;			// Current directory.
	uint256							uDirectEnd;			// Next order book.
	bool							bDirectAdvance;		// Need to advance directory.
	SLE::pointer					sleDirectDir;
	STAmount						saOfrRate;			// For correct ratio.

	// Node
	bool							bEntryAdvance;		// Need to advance entry.
	unsigned int					uEntry;
	uint256							uOfferIndex;
	SLE::pointer					sleOffer;
	uint160							uOfrOwnerID;
	bool							bFundsDirty;		// Need to refresh saOfferFunds, saTakerPays, & saTakerGets.
	STAmount						saOfferFunds;
	STAmount						saTakerPays;
	STAmount						saTakerGets;
};

// account id, currency id, issuer id :: node
typedef boost::tuple<uint160, uint160, uint160> aciSource;
typedef boost::unordered_map<aciSource, unsigned int>					curIssuerNode;	// Map of currency, issuer to node index.
typedef boost::unordered_map<aciSource, unsigned int>::const_iterator	curIssuerNodeConstIterator;

extern std::size_t hash_value(const aciSource& asValue);

// Holds a path state under incremental application.
class PathState
{
protected:
	Ledger::ref					mLedger;

	TER		pushNode(const int iType, const uint160& uAccountID, const uint160& uCurrencyID, const uint160& uIssuerID);
	TER		pushImply(const uint160& uAccountID, const uint160& uCurrencyID, const uint160& uIssuerID);

public:
	typedef boost::shared_ptr<PathState>		pointer;
	typedef const boost::shared_ptr<PathState>&	ref;

	TER							terStatus;
	std::vector<PaymentNode>	vpnNodes;

	// When processing, don't want to complicate directory walking with deletion.
	std::vector<uint256>		vUnfundedBecame;	// Offers that became unfunded or were completely consumed.

	// First time scanning foward, as part of path contruction, a funding source was mentioned for accounts. Source may only be
	// used there.
	curIssuerNode				umForward;			// Map of currency, issuer to node index.

	// First time working in reverse a funding source was used.
	// Source may only be used there if not mentioned by an account.
	curIssuerNode				umReverse;			// Map of currency, issuer to node index.

	LedgerEntrySet				lesEntries;

	int							mIndex;
	uint64						uQuality;			// 0 = none.
	const STAmount&				saInReq;			// --> Max amount to spend by sender.
	STAmount					saInAct;			// --> Amount spent by sender so far.
	STAmount					saInPass;			// <-- Amount spent by sender.
	const STAmount&				saOutReq;			// --> Amount to send.
	STAmount					saOutAct;			// --> Amount actually sent so far.
	STAmount					saOutPass;			// <-- Amount actually sent.

	PathState(
		const int				iIndex,
		const LedgerEntrySet&	lesSource,
		const STPath&			spSourcePath,
		const uint160&			uReceiverID,
		const uint160&			uSenderID,
		const STAmount&			saSend,
		const STAmount&			saSendMax
		);

	Json::Value	getJson() const;

	static PathState::pointer createPathState(
		const int				iIndex,
		const LedgerEntrySet&	lesSource,
		const STPath&			spSourcePath,
		const uint160&			uReceiverID,
		const uint160&			uSenderID,
		const STAmount&			saSend,
		const STAmount&			saSendMax
		)
	{
		return boost::make_shared<PathState>(iIndex, lesSource, spSourcePath, uReceiverID, uSenderID, saSend, saSendMax);
	}

	static bool lessPriority(PathState::ref lhs, PathState::ref rhs);
};

class RippleCalc
{
protected:
	LedgerEntrySet&					lesActive;

public:
	// First time working in reverse a funding source was mentioned.  Source may only be used there.
	curIssuerNode					mumSource;			// Map of currency, issuer to node index.

	// If the transaction fails to meet some constraint, still need to delete unfunded offers.
	boost::unordered_set<uint256>	musUnfundedFound;	// Offers that were found unfunded.

	PathState::pointer	pathCreate(const STPath& spPath);
	void				pathNext(PathState::ref pspCur, const int iPaths, const LedgerEntrySet& lesCheckpoint, LedgerEntrySet& lesCurrent);
	TER					calcNode(const unsigned int uIndex, PathState::ref pspCur, const bool bMultiQuality);
	TER					calcNodeRev(const unsigned int uIndex, PathState::ref pspCur, const bool bMultiQuality);
	TER					calcNodeFwd(const unsigned int uIndex, PathState::ref pspCur, const bool bMultiQuality);
	TER					calcNodeOfferRev(const unsigned int uIndex, PathState::ref pspCur, const bool bMultiQuality);
	TER					calcNodeOfferFwd(const unsigned int uIndex, PathState::ref pspCur, const bool bMultiQuality);
	TER					calcNodeAccountRev(const unsigned int uIndex, PathState::ref pspCur, const bool bMultiQuality);
	TER					calcNodeAccountFwd(const unsigned int uIndex, PathState::ref pspCur, const bool bMultiQuality);
	TER					calcNodeAdvance(const unsigned int uIndex, PathState::ref pspCur, const bool bMultiQuality, const bool bReverse);
	TER					calcNodeDeliverRev(
							const unsigned int			uIndex,
							PathState::ref				pspCur,
							const bool					bMultiQuality,
							const uint160&				uOutAccountID,
							const STAmount&				saOutReq,
							STAmount&					saOutAct);

	TER					calcNodeDeliverFwd(
							const unsigned int			uIndex,
							PathState::ref				pspCur,
							const bool					bMultiQuality,
							const uint160&				uInAccountID,
							const STAmount&				saInFunds,
							const STAmount&				saInReq,
							STAmount&					saInAct,
							STAmount&					saInFees);

	void				calcNodeRipple(const uint32 uQualityIn, const uint32 uQualityOut,
							const STAmount& saPrvReq, const STAmount& saCurReq,
							STAmount& saPrvAct, STAmount& saCurAct,
							uint64& uRateMax);

	RippleCalc(LedgerEntrySet& lesNodes) : lesActive(lesNodes) { ; }

	static TER rippleCalc(
		LedgerEntrySet&		lesActive,
			  STAmount&		saMaxAmountAct,
			  STAmount&		saDstAmountAct,
		const STAmount&		saDstAmountReq,
		const STAmount&		saMaxAmountReq,
		const uint160&		uDstAccountID,
		const uint160&		uSrcAccountID,
		const STPathSet&	spsPaths,
		const bool			bPartialPayment,
		const bool			bLimitQuality,
		const bool			bNoRippleDirect
		);
};

#endif
// vim:ts=4

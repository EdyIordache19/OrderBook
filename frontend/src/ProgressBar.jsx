import React from 'react';

const formatQty = (qty) => {
  if (qty >= 1000000) return (qty / 1000000).toFixed(2) + 'M';
  if (qty >= 1000) return (qty / 1000).toFixed(1) + 'K';
  return qty.toFixed(0);
};

const ProgressBar = ({ snapshot }) => {
  if (!snapshot) return <div>Loading...</div>;

  const totalAsks = snapshot.asks.reduce((acc, ask) => acc + Number(ask.qty), 0);
  const totalBids = snapshot.bids.reduce((acc, bid) => acc + Number(bid.qty), 0);
  const combinedTotal = totalAsks + totalBids;

  let asksPercentage = 50;
  let bidsPercentage = 50;
  if (combinedTotal > 0) {
    asksPercentage = (totalAsks / combinedTotal) * 100;
    bidsPercentage = (totalBids / combinedTotal) * 100;
  }

  const containerStyles = {
    height: 40,
    width: '100%',
    backgroundColor: "#161b22",
    borderRadius: 8,
    overflow: 'hidden',
    border: '1px solid #30363d',
    display: 'flex',
    marginTop: 10,
    boxShadow: '0 2px 4px rgba(0,0,0,0.3)',
  };

  const segmentStyles = {
    height: '100%',
    display: 'flex',
    flexDirection: 'column',
    alignItems: 'center',
    justifyContent: 'center',
    transition: 'width 0.4s ease-out',
    color: 'white',
    fontSize: '12px',
    whiteSpace: 'nowrap',
    padding: '0 8px',
    lineHeight: '1.2',
  };

  const asksStyles = {
    ...segmentStyles,
    width: `${asksPercentage}%`,
    backgroundColor: "#F6465D",
    borderTopLeftRadius: 8,
    borderBottomLeftRadius: 8,
  };

  const bidsStyles = {
    ...segmentStyles,
    width: `${bidsPercentage}%`,
    backgroundColor: "#0ECB81",
    borderTopRightRadius: 8,
    borderBottomRightRadius: 8,
  };

  const typeLabelStyles = {
    fontWeight: 'bold',
    textTransform: 'uppercase',
    letterSpacing: '0.5px',
  };

  return (
    <div style={{ padding: '10px 0' }}>
      {/* Title Header above the bar */}
      <div style={{ color: '#8b949e', fontSize: '13px', fontWeight: 'bold', marginBottom: '8px', textAlign: 'center' }}>
        Asks / Bids
      </div>

      {/* The main progress bar container */}
      <div style={containerStyles}>
        {/* ASKS Section */}
        <div style={asksStyles}>
          <div style={typeLabelStyles}>Asks</div>
          <div>{asksPercentage.toFixed(0)}% ({formatQty(totalAsks)})</div>
        </div>

        {/* BIDS Section */}
        <div style={bidsStyles}>
          <div style={typeLabelStyles}>Bids</div>
          <div>{bidsPercentage.toFixed(0)}% ({formatQty(totalBids)})</div>
        </div>
      </div>
    </div>
  );
};

export default ProgressBar;
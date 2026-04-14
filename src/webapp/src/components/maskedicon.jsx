const MaskedIcon = ({ path, height, color }) => {
  return (
    <div
      className={`${color}`}
      style={{
        WebkitMask: `url(${path}) no-repeat center / contain`,
        mask: `url(${path}) no-repeat center / contain`,
        width: `auto`,
        height: height,
      }}
    >
      <img
        className="w-full h-full opacity-0"
        src={path}
        onError={(event) => {
          event.currentTarget.parentElement.style.display = "none";
        }}
      />
    </div>
  );
};

export default MaskedIcon;

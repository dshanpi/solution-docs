import React from 'react';

// 原创概念插画，表达设备、板卡和浏览器之间的关系，不作为实物接线图。
export default function HeroArtwork() {
  return <svg viewBox="0 0 660 540" role="img" aria-labelledby="kvm-art-title" xmlns="http://www.w3.org/2000/svg">
    <title id="kvm-art-title">KVM 概念插画：远端设备通过 KVM 板卡，将画面和键鼠操作连接到浏览器</title>
    <g fill="none" stroke="#91b435" strokeWidth="2">
      <path d="M40 63h90v65H40zM60 140h50M86 128v12M534 408h93v65h-93zM552 486h58M582 473v13"/>
      <path d="M144 39h74v50h-74zM176 89v11h-19M19 186h44v70H19zM26 239h30"/>
      <path d="M250 30h40M270 10v40M586 74h36M604 56v36M623 317h20M633 307v20"/>
      <circle cx="87" cy="367" r="19"/><circle cx="593" cy="235" r="12"/>
    </g>
    <g transform="translate(217 51) rotate(6 180 150)">
      <rect x="20" y="-14" width="344" height="249" rx="9" fill="#a8d435" stroke="#15200d" strokeWidth="2"/>
      <rect x="9" y="-5" width="344" height="249" rx="9" fill="#eaf6c9" stroke="#15200d" strokeWidth="2"/>
      <rect width="344" height="249" rx="9" fill="#f6f7e9" stroke="#15200d" strokeWidth="3"/>
      <path d="M0 32h344" stroke="#15200d" strokeWidth="2"/>
      <g fill="#15200d"><circle cx="17" cy="16" r="3"/><circle cx="29" cy="16" r="3"/><circle cx="41" cy="16" r="3"/></g>
      <text x="79" y="21" fill="#15200d" fontSize="12" fontFamily="monospace">WEB KVM / YOUR DEVICE</text>
      <rect x="15" y="47" width="314" height="186" fill="#19270f"/>
      <path d="M15 182 92 103l65 64 62-93 110 116v43H15z" fill="#81a72b"/>
      <path d="m63 233 71-94 73 94zm147 0 56-66 63 66" fill="#c8f343"/>
      <circle cx="278" cy="87" r="19" fill="#c8f343"/>
      <path d="m231 145 9 61 15-19 22-5z" fill="#fafbee" stroke="#15200d" strokeWidth="3"/>
      <path d="m268 187 11 24" stroke="#fafbee" strokeWidth="7"/>
      <rect x="24" y="57" width="69" height="20" rx="10" fill="#f6f7e9"/>
      <circle cx="35" cy="67" r="3" fill="#557723"/><text x="44" y="71" fontSize="10" fill="#15200d" fontFamily="monospace">DESKTOP</text>
    </g>
    <g fill="none" stroke="#15200d" strokeWidth="4" strokeLinecap="round">
      <path d="M466 302v26q0 24-24 24h-54"/>
      <path d="m405 345-9 7 9 7" strokeWidth="2"/>
      <path d="M568 291v158q0 36-36 36H420"/>
      <path d="M166 351h42q20 0 20 20v7h46"/>
      <path d="M259 433h-33q-22 0-22-22v-20q0-20-20-20h-19" strokeWidth="2"/>
    </g>
    <g transform="translate(242 310) rotate(-9 92 75)">
      <path d="M0 12 185 0l18 140-186 16z" fill="#17240e"/>
      <path d="m8 19 169-10 17 122-168 15z" fill="#355321" stroke="#17240e" strokeWidth="2"/>
      <g fill="none" stroke="#9bbf55" strokeWidth="2"><path d="M44 40h27v23M33 105h35V83M122 41v17h34M124 99h26v18M104 27v26M86 112v18"/></g>
      <path d="M64 48h65v65H64z" fill="#c8f343" stroke="#17240e" strokeWidth="3"/>
      <text x="77" y="88" fontSize="20" fontFamily="monospace" fontWeight="bold" fill="#17240e">KVM</text>
      <g fill="#f4f6e8"><path d="M23 33h13v29H23zM18 83h20v32H18zM149 64h24v31h-24z"/><circle cx="48" cy="126" r="4"/><circle cx="157" cy="24" r="4"/></g>
      <g stroke="#17240e" strokeWidth="3"><path d="M74 42v-9m12 9v-9m12 9v-9m12 9v-9M73 118v10m12-10v10m12-10v10m12-10v10M59 59H49m10 13H49m10 13H49m10 13H49m85-39h10m-10 13h10m-10 13h10m-10 13h10"/></g>
    </g>
    <g transform="translate(38 252) rotate(-8 67 58)">
      <path d="M0 0h142v100H0z" fill="#f6f7e9" stroke="#15200d" strokeWidth="3"/>
      <path d="M10 10h122v76H10z" fill="#18260f"/>
      <path d="m26 27 12 10-12 10m22 0h33" fill="none" stroke="#c8f343" strokeWidth="3"/>
      <path d="M62 102v20H30m50-20v20h32" fill="none" stroke="#15200d" strokeWidth="4"/>
      <text x="12" y="151" fontSize="12" fontFamily="monospace" fill="#15200d">REMOTE DEVICE</text>
    </g>
    <g transform="translate(384 459) rotate(7)"><path d="M0 0h37v43q0 20-18 20T0 43z" fill="#f6f7e9" stroke="#15200d" strokeWidth="3"/><path d="M19 0v24M0 24h37" stroke="#15200d" strokeWidth="2"/></g>
    <g fill="#15200d" fontFamily="monospace" fontSize="12"><text x="480" y="346">NETWORK</text><text x="190" y="335">HDMI</text><text x="192" y="425">USB</text></g>
    <path d="m525 374 8 18 19 2-14 13 4 19-17-10-17 10 4-19-14-13 19-2z" fill="#15200d"/>
    <path d="M147 447v37m-18-18h36m-31-13 26 26m0-26-26 26" stroke="#15200d" strokeWidth="3"/>
  </svg>;
}
